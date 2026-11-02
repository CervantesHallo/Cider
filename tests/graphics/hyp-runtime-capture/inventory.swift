import Foundation
import AppKit
import ScreenCaptureKit
import CoreGraphics
import CryptoKit
import Darwin

@main struct Inventory {
 @MainActor static func main() async {
  do {
   _ = NSApplication.shared
   guard CommandLine.arguments.count == 2 else { throw NSError(domain:"arguments",code:1) }
   let engine=URL(fileURLWithPath:CommandLine.arguments[1]).standardizedFileURL
   guard engine.path.hasPrefix("/Users/cervantes/Cider/out/waitall-mac-20261018-admission/private-home/AppSupport/Engines/"),engine.resolvingSymlinksInPath().path==engine.path,CGPreflightScreenCaptureAccess() else { throw NSError(domain:"private-engine-or-existing-permission",code:1) }
   let begin=DispatchTime.now().uptimeNanoseconds
   let content=try await SCShareableContent.excludingDesktopWindows(true,onScreenWindowsOnly:false)
   guard DispatchTime.now().uptimeNanoseconds-begin<20_000_000_000 else { throw NSError(domain:"metadata-deadline",code:1) }
   var rows:[[String:Any]]=[]
   for window in content.windows {
    guard let app=window.owningApplication,app.processID>0 else { continue }
    var path=[CChar](repeating:0,count:4096)
    let read=path.withUnsafeMutableBytes { proc_pidpath(app.processID,$0.baseAddress,UInt32($0.count)) }
    guard read>0,let executable=String(bytes:path.prefix { $0 != 0 }.map { UInt8(bitPattern:$0) },encoding:.utf8),executable.hasPrefix(engine.path+"/") else { continue }
    var info=proc_bsdinfo()
    guard proc_pidinfo(app.processID,PROC_PIDTBSDINFO,0,&info,Int32(MemoryLayout<proc_bsdinfo>.size))==MemoryLayout<proc_bsdinfo>.size,info.pbi_uid==geteuid(),info.pbi_start_tvsec<UInt64.max/1_000_000 else { continue }
    let title=window.title ?? ""
    rows.append(["window":window.windowID,"ownerPID":app.processID,"birthMicros":info.pbi_start_tvsec*1_000_000+info.pbi_start_tvusec,"nativeExecutable":executable,"title":title,"titleSHA256":SHA256.hash(data:Data(title.utf8)).map { String(format:"%02x",$0) }.joined(),"x":window.frame.origin.x,"y":window.frame.origin.y,"width":window.frame.width,"height":window.frame.height,"onScreen":window.isOnScreen])
   }
   let data=try JSONSerialization.data(withJSONObject:["scope":"private-engine test-window metadata only; no screenshot","windows":rows],options:[.sortedKeys]);FileHandle.standardOutput.write(data+Data([10]))
  } catch { FileHandle.standardError.write(Data("inventory failed: \(error)\n".utf8));exit(1) }
 }
}
