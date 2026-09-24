import Foundation
import CEWAF

struct CoreFailure: Error, Decodable, LocalizedError {
    let code: String
    let message: String
    var errorDescription: String? { message }
}
private struct CoreEnvelope<T: Decodable>: Decodable {
    let ok: Bool
    let value: T?
    let error: CoreFailure?
}
enum RustCore {
    static func call<T: Decodable>(_ request: [String: Any], as type: T.Type = T.self) throws -> T {
        let bytes = try JSONSerialization.data(withJSONObject: request)
        let response = bytes.withUnsafeBytes { ewaf_request($0.bindMemory(to: UInt8.self).baseAddress, bytes.count) }!
        defer { ewaf_string_free(response) }
        let data = Data(bytes: response, count: strlen(response))
        let envelope = try JSONDecoder().decode(CoreEnvelope<T>.self, from: data)
        if let value = envelope.value, envelope.ok { return value }
        let error = envelope.error ?? CoreFailure(code: "invalid_response", message: "The shared core returned an invalid response.")
        switch error.code {
        case "invalid_date": throw PlanError.invalidDate
        case "reversed_range": throw PlanError.reversedRange
        default: throw error
        }
    }
}
public func validateExactDates(start: String, end: String) throws -> (CivilDate, CivilDate) {
    struct Dates: Decodable { let start: String; let end: String }
    let dates: Dates = try RustCore.call(["op": "exact", "start": start, "end": end])
    return (try CivilDate(folderName: dates.start), try CivilDate(folderName: dates.end))
}
