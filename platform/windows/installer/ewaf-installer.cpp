// SPDX-FileCopyrightText: Thomas Lothian
// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <wintrust.h>
#include <softpub.h>
#include <msi.h>
#include <msiquery.h>
#include <tlhelp32.h>
#include <bcrypt.h>

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "msi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "bcrypt.lib")

class VerificationException : public std::exception {
public:
    VerificationException(std::string code, std::string message)
        : m_code(std::move(code)), m_message(std::move(message)) {}

    const std::string &code() const { return m_code; }
    const std::string &message() const { return m_message; }
    const char *what() const noexcept override { return m_message.c_str(); }

private:
    std::string m_code;
    std::string m_message;
};

static std::wstring toWide(const std::string &str) {
    if (str.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
    std::wstring wstr(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &wstr[0], size);
    return wstr;
}

static std::string toUtf8(const std::wstring &wstr) {
    if (wstr.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string str(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &str[0], size, nullptr, nullptr);
    return str;
}

static std::string escapeJson(const std::string &s) {
    std::string out;
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

static std::string computeSha256Hex(HANDLE hFile) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) {
        throw VerificationException("runtime", "Could not initialize SHA256 provider.");
    }

    DWORD hashSize = 32;
    DWORD cbData = 0;
    std::vector<BYTE> hash(hashSize);

    if (BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0) != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        throw VerificationException("runtime", "Could not create SHA256 hash.");
    }

    SetFilePointer(hFile, 0, nullptr, FILE_BEGIN);
    BYTE buffer[65536];
    DWORD bytesRead = 0;
    while (ReadFile(hFile, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0) {
        BCryptHashData(hHash, buffer, bytesRead, 0);
    }

    BCryptFinishHash(hHash, hash.data(), hashSize, 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    static const char hexChars[] = "0123456789ABCDEF";
    std::string hex;
    hex.reserve(hashSize * 2);
    for (BYTE b : hash) {
        hex += hexChars[(b >> 4) & 0xF];
        hex += hexChars[b & 0xF];
    }
    return hex;
}

static void verifyPublisher(const std::wstring &path, const std::string &expectedPublisher) {
    WINTRUST_FILE_INFO fileInfo{};
    fileInfo.cbStruct = sizeof(fileInfo);
    fileInfo.pcwszFilePath = path.c_str();

    WINTRUST_DATA trustData{};
    trustData.cbStruct = sizeof(trustData);
    trustData.dwUIChoice = WTD_UI_NONE;
    trustData.fdwRevocationChecks = WTD_REVOKE_WHOLECHAIN;
    trustData.dwUnionChoice = WTD_CHOICE_FILE;
    trustData.pFile = &fileInfo;
    trustData.dwStateAction = WTD_STATEACTION_VERIFY;
    trustData.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;

    GUID policyGuid = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    LONG status = WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &policyGuid, &trustData);

    trustData.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &policyGuid, &trustData);

    if (status != ERROR_SUCCESS) {
        throw VerificationException("signature", "Update publisher signature is invalid.");
    }

    HCERTSTORE hStore = nullptr;
    HCRYPTMSG hMsg = nullptr;
    DWORD dwEncoding = 0, dwContentType = 0, dwFormatType = 0;

    if (!CryptQueryObject(CERT_QUERY_OBJECT_FILE, path.c_str(),
                          CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
                          CERT_QUERY_FORMAT_FLAG_BINARY, 0,
                          &dwEncoding, &dwContentType, &dwFormatType,
                          &hStore, &hMsg, nullptr)) {
        throw VerificationException("signature", "Could not extract publisher certificate.");
    }

    DWORD signerSize = 0;
    CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, nullptr, &signerSize);
    std::vector<BYTE> signerData(signerSize);
    CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, signerData.data(), &signerSize);
    auto *signerInfo = reinterpret_cast<CMSG_SIGNER_INFO *>(signerData.data());

    CERT_INFO certInfo{};
    certInfo.Issuer = signerInfo->Issuer;
    certInfo.SerialNumber = signerInfo->SerialNumber;

    PCCERT_CONTEXT pCertContext = CertFindCertificateInStore(
        hStore, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0,
        CERT_FIND_SUBJECT_CERT, &certInfo, nullptr);

    if (!pCertContext) {
        if (hMsg) CryptMsgClose(hMsg);
        if (hStore) CertCloseStore(hStore, 0);
        throw VerificationException("publisher", "Signer certificate not found in store.");
    }

    DWORD nameLen = CertNameToStrA(pCertContext->dwCertEncodingType, &pCertContext->pCertInfo->Subject,
                                   CERT_X500_NAME_STR, nullptr, 0);
    std::string subjectName(nameLen, '\0');
    CertNameToStrA(pCertContext->dwCertEncodingType, &pCertContext->pCertInfo->Subject,
                   CERT_X500_NAME_STR, &subjectName[0], nameLen);
    if (!subjectName.empty() && subjectName.back() == '\0') subjectName.pop_back();

    CertFreeCertificateContext(pCertContext);
    if (hMsg) CryptMsgClose(hMsg);
    if (hStore) CertCloseStore(hStore, 0);

    if (expectedPublisher.empty() || subjectName != expectedPublisher) {
        throw VerificationException("publisher", "Update publisher does not match the enrolled identity.");
    }
}

static void checkMsi(UINT result, const std::string &msg) {
    if (result != ERROR_SUCCESS) {
        throw VerificationException("identity", msg + " (" + std::to_string(result) + ")");
    }
}

static std::string getMsiProperty(MSIHANDLE hDatabase, const std::wstring &name) {
    std::wstring query = L"SELECT `Value` FROM `Property` WHERE `Property`='" + name + L"'";
    MSIHANDLE hView = 0;
    checkMsi(MsiDatabaseOpenViewW(hDatabase, query.c_str(), &hView), "MsiDatabaseOpenView failed");
    checkMsi(MsiViewExecute(hView, 0), "MsiViewExecute failed");

    MSIHANDLE hRecord = 0;
    UINT fetchRes = MsiViewFetch(hView, &hRecord);
    if (fetchRes != ERROR_SUCCESS) {
        MsiCloseHandle(hView);
        return {};
    }

    WCHAR buf[1024];
    DWORD cch = 1023;
    checkMsi(MsiRecordGetStringW(hRecord, 1, buf, &cch), "MsiRecordGetString failed");

    MsiCloseHandle(hRecord);
    MsiCloseHandle(hView);
    return toUtf8(std::wstring(buf, cch));
}

static void verifyIdentity(const std::wstring &path, const std::string &version, const std::string &arch) {
    MSIHANDLE hDatabase = 0;
    // MSIDBOPEN_READONLY is a null pointer; pass it directly to the Unicode API.
    checkMsi(MsiOpenDatabaseW(path.c_str(), nullptr, &hDatabase), "Could not open MSI database");

    MSIHANDLE hSummary = 0;
    checkMsi(MsiGetSummaryInformationW(hDatabase, nullptr, 0, &hSummary), "Could not read MSI summary info");

    UINT uiDataType = 0;
    INT iValue = 0;
    FILETIME ftValue{};
    WCHAR templateBuf[1024];
    DWORD cchTemplate = 1023;
    // The Template summary property (PID 7) holds "x64;language" or "Arm64;language".
    constexpr UINT kTemplateSummaryPropertyId = 7;
    checkMsi(MsiSummaryInfoGetPropertyW(hSummary, kTemplateSummaryPropertyId, &uiDataType, &iValue, &ftValue, templateBuf, &cchTemplate),
             "Could not read MSI architecture property");
    MsiCloseHandle(hSummary);

    std::string templateStr = toUtf8(std::wstring(templateBuf, cchTemplate));
    std::string msiArch = templateStr.substr(0, templateStr.find(';'));
    std::string targetArch = arch;
    std::transform(msiArch.begin(), msiArch.end(), msiArch.begin(), ::tolower);
    std::transform(targetArch.begin(), targetArch.end(), targetArch.begin(), ::tolower);

    if ((targetArch != "x64" && targetArch != "arm64") || msiArch != targetArch) {
        MsiCloseHandle(hDatabase);
        throw VerificationException("identity", "MSI architecture mismatch.");
    }

    std::string upgradeCode = getMsiProperty(hDatabase, L"UpgradeCode");
    std::string prodName = getMsiProperty(hDatabase, L"ProductName");
    std::string prodVer = getMsiProperty(hDatabase, L"ProductVersion");
    std::string manufacturer = getMsiProperty(hDatabase, L"Manufacturer");

    MsiCloseHandle(hDatabase);

    if (upgradeCode != "{33C1F535-C25C-48C6-BDE6-F186ABFCB085}" ||
        prodName != "EWAF" || prodVer != version || manufacturer != "TLO Labs") {
        throw VerificationException("identity", "MSI belongs to another application or version.");
    }
}

static bool isEwafRunning() {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    bool running = false;
    if (Process32FirstW(hSnap, &pe)) {
        do {
            std::wstring name = pe.szExeFile;
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            if (name == L"ewaf.exe" || name == L"ewaf") {
                running = true;
                break;
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return running;
}

int main(int argc, char *argv[]) {
    bool coordinated = false;
    try {
        bool verifyOnly = (argc == 7 && std::string(argv[1]) == "--verify-only");
        bool installMode = (argc == 9 && std::string(argv[1]) == "--install");

        if (!verifyOnly && !installMode) {
            throw VerificationException("arguments", "Expected mode, MSI, digest, publisher, version, architecture and optional parent ID/start time.");
        }

        std::string pkgStr = argv[2];
        std::wstring pkgPath = toWide(pkgStr);
        DWORD attrs = GetFileAttributesW(pkgPath.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES || pkgStr.size() < 4 ||
            pkgStr.substr(pkgStr.size() - 4) != ".msi") {
            throw VerificationException("package", "Expected an MSI update.");
        }

        HANDLE hFile = CreateFileW(pkgPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) {
            throw VerificationException("package", "Could not open MSI file.");
        }

        std::string computedDigest = computeSha256Hex(hFile);
        std::string expectedDigest = argv[3];
        std::string expectedPub = argv[4];
        std::string expectedVer = argv[5];
        std::string expectedArch = argv[6];

        std::string c1 = computedDigest, c2 = expectedDigest;
        std::transform(c1.begin(), c1.end(), c1.begin(), ::tolower);
        std::transform(c2.begin(), c2.end(), c2.begin(), ::tolower);
        if (c1 != c2) {
            CloseHandle(hFile);
            throw VerificationException("digest", "Update digest changed.");
        }

        verifyPublisher(pkgPath, expectedPub);
        verifyIdentity(pkgPath, expectedVer, expectedArch);

        if (verifyOnly) {
            CloseHandle(hFile);
            std::cout << "{\"verified\":true}" << std::endl;
            return 0;
        }

        int parentId = std::stoi(argv[7]);
        long long parentStarted = std::stoll(argv[8]);

        HANDLE hParent = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, parentId);
        if (!hParent) {
            CloseHandle(hFile);
            throw VerificationException("coordinator", "The original EWAF process is no longer running.");
        }

        FILETIME ftCreate{}, ftExit{}, ftKernel{}, ftUser{};
        GetProcessTimes(hParent, &ftCreate, &ftExit, &ftKernel, &ftUser);
        ULARGE_INTEGER uli;
        uli.LowPart = ftCreate.dwLowDateTime;
        uli.HighPart = ftCreate.dwHighDateTime;
        long long startedTicks = static_cast<long long>(uli.QuadPart) + 504911232000000000LL;

        if (startedTicks != parentStarted) {
            CloseHandle(hParent);
            CloseHandle(hFile);
            throw VerificationException("coordinator", "The original EWAF process start time mismatch.");
        }

        std::cout << "READY" << std::endl;
        std::cout.flush();

        std::string command;
        if (!std::getline(std::cin, command) || command != "COMMIT") {
            CloseHandle(hParent);
            CloseHandle(hFile);
            return 0;
        }

        coordinated = true;
        WaitForSingleObject(hParent, 120000);
        CloseHandle(hParent);

        if (isEwafRunning()) {
            CloseHandle(hFile);
            throw VerificationException("coordinator", "Close all EWAF instances before installation.");
        }

        CloseHandle(hFile);

        WCHAR sysDir[MAX_PATH];
        GetSystemDirectoryW(sysDir, MAX_PATH);
        std::wstring msiexecPath = std::wstring(sysDir) + L"\\msiexec.exe";
        std::wstring cmdLine = L"\"" + msiexecPath + L"\" /i \"" + pkgPath + L"\" /passive REBOOT=ReallySuppress MSIRESTARTMANAGERCONTROL=Disable";

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        if (!CreateProcessW(nullptr, &cmdLine[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
            throw VerificationException("installation", "Could not start Windows Installer.");
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        if (exitCode != 0 && exitCode != 3010) {
            throw VerificationException("installation", "Windows Installer failed (" + std::to_string(exitCode) + "). Its transaction restores the previous installation on failure.");
        }

        if (exitCode == 3010) {
            MessageBoxW(nullptr, L"Windows requires a restart to finish this update. Save your work and restart Windows before reopening EWAF.",
                        L"EWAF update", MB_OK | MB_ICONINFORMATION);
        }

        DeleteFileW(pkgPath.c_str());
        return 0;
    } catch (const VerificationException &v) {
        std::cerr << "{\"verified\":false,\"code\":\"" << escapeJson(v.code())
                  << "\",\"error\":\"" << escapeJson(v.message()) << "\"}" << std::endl;
        if (coordinated) {
            MessageBoxA(nullptr, v.message().c_str(), "EWAF update could not be installed", MB_OK | MB_ICONERROR);
        }
        return 1;
    } catch (const std::exception &e) {
        std::cerr << "{\"verified\":false,\"code\":\"runtime\",\"error\":\"" << escapeJson(e.what()) << "\"}" << std::endl;
        if (coordinated) {
            MessageBoxA(nullptr, e.what(), "EWAF update could not be installed", MB_OK | MB_ICONERROR);
        }
        return 1;
    }
}

#else
int main() { return 0; }
#endif
