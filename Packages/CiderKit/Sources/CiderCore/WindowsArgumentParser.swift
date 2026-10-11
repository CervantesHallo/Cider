/// Parses an argument tail with Microsoft's C runtime quoting rules.
/// The executable is supplied separately, so argv[0]'s special rules do not apply.
/// This is not a cmd.exe/PowerShell interpreter or a full CommandLineToArgvW clone.
/// Contract: https://learn.microsoft.com/en-us/cpp/c-language/parsing-c-command-line-arguments
public enum WindowsArgumentParser {
    public static func parse(_ line: String) -> [String] {
        // A Windows command-line buffer ends at its first NUL. Never turn the
        // unreachable suffix into additional POSIX argv entries during launch.
        let input = Array(line.unicodeScalars.prefix { $0.value != 0 })
        var index = 0
        var arguments: [String] = []

        func isSeparator(_ scalar: Unicode.Scalar) -> Bool {
            scalar.value == 0x20 || scalar.value == 0x09
        }

        while index < input.count {
            while index < input.count, isSeparator(input[index]) { index += 1 }
            guard index < input.count else { break }

            var argument = ""
            var quoted = false
            while index < input.count {
                if !quoted, isSeparator(input[index]) { break }

                var backslashes = 0
                while index < input.count, input[index].value == 0x5c {
                    backslashes += 1
                    index += 1
                }

                if index < input.count, input[index].value == 0x22 {
                    argument += String(repeating: "\\", count: backslashes / 2)
                    if backslashes % 2 == 1 {
                        // The final unpaired backslash escapes the quote.
                        argument += "\""
                        index += 1
                    } else if quoted, index + 1 < input.count, input[index + 1].value == 0x22 {
                        // Within a quoted argument, two quotes produce one literal quote.
                        argument += "\""
                        index += 2
                    } else {
                        quoted.toggle()
                        index += 1
                    }
                } else {
                    // Backslashes only escape quotes; preserve runs before other characters/end.
                    argument += String(repeating: "\\", count: backslashes)
                    if index == input.count { break }
                    if !quoted, isSeparator(input[index]) { break }
                    argument.unicodeScalars.append(input[index])
                    index += 1
                }
            }
            // A syntactically present argument can be empty (""). An unmatched quote
            // consumes the remaining input, following the runtime's documented behavior.
            arguments.append(argument)
        }
        return arguments
    }
}
