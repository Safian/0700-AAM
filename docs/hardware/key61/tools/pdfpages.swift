import PDFKit
import Foundation
let a = CommandLine.arguments
guard a.count == 3, let d = PDFDocument(url: URL(fileURLWithPath: a[1])) else { print("open failed"); exit(1) }
try? FileManager.default.createDirectory(atPath: a[2], withIntermediateDirectories: true)
for i in 0..<d.pageCount {
    let t = d.page(at: i)?.string ?? ""
    try t.write(toFile: String(format: "%@/%03d.txt", a[2], i + 1), atomically: true, encoding: .utf8)
}
print("pages:", d.pageCount)
