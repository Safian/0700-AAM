import PDFKit
import Foundation
let a = CommandLine.arguments
guard a.count == 3, let d = PDFDocument(url: URL(fileURLWithPath: a[1])) else { print("open failed"); exit(1) }
try d.string?.write(toFile: a[2], atomically: true, encoding: .utf8)
print("pages:", d.pageCount)
