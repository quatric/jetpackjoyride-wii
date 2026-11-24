import CoreGraphics
import Foundation
// usage: winid <pid>  -> prints the CGWindowID of the largest on-screen window of that pid
let pid = Int32(CommandLine.arguments[1])!
let list = CGWindowListCopyWindowInfo([.optionOnScreenOnly], kCGNullWindowID) as! [[String: Any]]
var best = (0, 0.0)
for w in list where (w[kCGWindowOwnerPID as String] as? Int32) == pid {
    let b = w[kCGWindowBounds as String] as! [String: Double]
    let a = (b["Width"] ?? 0) * (b["Height"] ?? 0)
    if a > best.1 { best = (w[kCGWindowNumber as String] as! Int, a) }
}
print(best.0)
