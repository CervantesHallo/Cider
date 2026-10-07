import Foundation

// The kernel-API fidelity matrix (docs/plan/09 §3): for every Windows kernel API Cider cares about,
// what Wine does today, what Windows documents, whether Wine's user-mode driver model can deliver it,
// and what the honest handling is. Audited against the engine's own Wine tree; see `generatedFrom`.
//
// This type exists to make one rule mechanical rather than aspirational: an API Wine structurally
// cannot deliver in the audited implementation must be dispositioned `honestFailure`. Claiming it is `implemented` is the
// success-faking ADR-010 forbids, so a matrix that does it fails to load.

public struct KernelFidelity: Codable, Sendable, Equatable {
    /// Reachability in the recorded build and its current host/protocol model.
    /// New protocol or host implementations require fresh contract evidence and a revised audit.
    public enum Reachability: String, Codable, Sendable {
        case faithful      // the contract can be met in full
        case partial       // part of the contract can be met; the rest must fail honestly
        case unreachable   // the audited implementation cannot satisfy this contract
    }

    /// How Cider intends the API to behave. Never "return success without doing the work".
    public enum Disposition: String, Codable, Sendable {
        case implemented
        case honestFailure = "honest_failure"
        case needsImplementation = "needs_implementation"
    }

    /// What the audited Wine tree does *today*.
    public enum Severity: String, Codable, Sendable {
        case implemented
        case honestFailure = "honest_failure"
        case deceptiveSuccess = "deceptive_success"   // returns success having done nothing — a red line
        case missing
    }

    public struct Entry: Codable, Sendable, Equatable {
        public var symbol: String
        public var dll: String
        public var location: String                   // "dlls/ntoskrnl.exe/ntoskrnl.c:3152"
        public var wineBehaviour: String
        public var windowsContract: String
        public var reachability: Reachability
        public var disposition: Disposition
        public var severity: Severity
        public var notes: String?

        enum CodingKeys: String, CodingKey {
            case symbol, dll, location, reachability, disposition, severity, notes
            case wineBehaviour = "wine_behaviour"
            case windowsContract = "windows_contract"
        }
    }

    public var schema: String
    public var revision: Int
    public var generatedFrom: String                  // the Wine tree the audit was made against
    public var audited: String                        // yyyy-MM-dd
    public var wineModelConstraints: String
    public var ndisSummary: String?
    public var wdfStatus: String?
    public var upstreamMovement: String?
    public var entries: [Entry]

    enum CodingKeys: String, CodingKey {
        case schema, revision, audited, entries
        case generatedFrom = "generated_from"
        case wineModelConstraints = "wine_model_constraints"
        case ndisSummary = "ndis_summary"
        case wdfStatus = "wdf_status"
        case upstreamMovement = "upstream_movement"
    }

    public func entry(for symbol: String) -> Entry? { entries.first { $0.symbol == symbol } }

    /// APIs the audited tree fakes success for. These are the K-track's first target: each one is
    /// either implemented for real or changed to report an honest failure.
    public var deceptive: [Entry] { entries.filter { $0.severity == .deceptiveSuccess } }

    /// Contracts unavailable in the audited implementation; this is not a claim about all future architectures.
    public var unreachable: [Entry] { entries.filter { $0.reachability == .unreachable } }
}

public enum KernelFidelityPolicy {
    /// Rejects a matrix that overstates what Wine can do. Returns the reason, or nil when clean.
    public static func violation(in matrix: KernelFidelity) -> String? {
        var seen = Set<String>()
        for e in matrix.entries {
            let id = "\(e.dll)!\(e.symbol)"
            guard seen.insert(id).inserted else { return "duplicate entry for \(id)" }

            // The invariant this whole type exists for (ADR-010, docs/plan/09 §2).
            if e.reachability == .unreachable && e.disposition != .honestFailure {
                return "\(id) is unreachable in Wine's driver model but dispositioned "
                     + "\(e.disposition.rawValue); the only honest option is honest_failure"
            }
            if e.reachability == .unreachable && e.severity == .implemented {
                return "\(id) is unreachable but recorded as already implemented"
            }
            // Calling a stub that fakes success "implemented" is the deception itself.
            if e.disposition == .implemented && e.severity != .implemented {
                return "\(id) is dispositioned implemented but the tree records it as "
                     + "\(e.severity.rawValue)"
            }
        }
        return nil
    }
}
