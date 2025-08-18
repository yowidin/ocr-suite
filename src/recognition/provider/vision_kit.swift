import AppKit
import CoreGraphics
import CoreImage
import Foundation
import Vision

private func clamp(rect: CGRect) -> CGRect {
    let clampedX = max(0.0, min(1.0, rect.origin.x))
    let clampedY = max(0.0, min(1.0, rect.origin.y))

    let maxWidth = 1.0 - clampedX
    let maxHeight = 1.0 - clampedY

    let clampedWidth = max(0.0, min(maxWidth, rect.size.width))
    let clampedHeight = max(0.0, min(maxHeight, rect.size.height))

    return CGRect(x: clampedX, y: clampedY, width: clampedWidth, height: clampedHeight)
}

struct RecognitionEntry {
    let text: String
    let left: UInt32
    let top: UInt32
    let right: UInt32
    let bottom: UInt32
    let confidence: Float

    /// Convert a recognized text oservation into internal representation:
    /// - VisionKit uses the 0..1 coordinate space, we want to have absolute coordinates in image space
    /// - VisionKit uses the bottom-left corner as an origin, we want to convert to top-left
    /// - VisionKit returns full sentences, we want to split those in words
    /// - We want to filter out short words
    static func from(_ observation: VNRecognizedTextObservation, _ imageWidth: CGFloat, _ imageHeight: CGFloat)
        -> [RecognitionEntry]
    {
        guard let topCandidate = observation.topCandidates(1).first else { return [] }

        struct WordWithRange {
            let word: String
            let range: Range<String.Index>
        }

        let allText = topCandidate.string
        var allWords: [WordWithRange] = []
        let splitOptions: String.EnumerationOptions = [
          String.EnumerationOptions.byWords,
          String.EnumerationOptions.localized
        ]
        allText.enumerateSubstrings(in: allText.startIndex..<allText.endIndex, options: splitOptions) {
            (substring, range, _, _) in
            if let word = substring {
                allWords.append(WordWithRange(word: word, range: range))
            }
        }

        var result: [RecognitionEntry] = []
        for entry in allWords {
            if entry.word.count < 3 { continue }

            // boundingBox might fail so we have to unwrap the optional first
            guard let box = try? topCandidate.boundingBox(for: entry.range) else {
                continue
            }

            // boundingBox result itself might also be nil
            guard let box = box else {
                continue
            }

            // For some reason voth the boundingBox function and the observation.boundingBox member sometimes returns negative values.
            // The only thing we can do is clamp the values.
            let boundingBox = clamp(rect: box.boundingBox)

            let textWidth = CGFloat(boundingBox.size.width)
            let textHeight = CGFloat(boundingBox.size.height)

            let x = boundingBox.origin.x
            let y = 1.0 - boundingBox.origin.y - boundingBox.size.height

            let cgLeft = x * CGFloat(imageWidth)
            let cgTop = y * CGFloat(imageHeight)

            let normalized =
                RecognitionEntry.init(
                    text: entry.word,
                    left: UInt32(cgLeft),
                    top: UInt32(cgTop),
                    right: UInt32(cgLeft + textWidth * CGFloat(imageWidth)),
                    bottom: UInt32(cgTop + textHeight * CGFloat(imageHeight)),
                    confidence: topCandidate.confidence * 100.0
                )

            result.append(normalized)
        }

        return result
    }
}

func saveCGImage(_ image: CGImage, _ path: String) {
    let bitmapRep = NSBitmapImageRep(cgImage: image)
    let properties: [NSBitmapImageRep.PropertyKey: Any] = [:]
    if let pngData = bitmapRep.representation(using: .png, properties: properties) {
        do {
            try pngData.write(to: URL(fileURLWithPath: path))
            print("Image saved successfully to: \(path)")
        } catch {
            print("Error saving image: \(error.localizedDescription)")
        }
    }
}

func createCGImage(
    from imageData: UnsafePointer<UInt8>,
    width: UInt32,
    height: UInt32,
    bytesPerLine: UInt32
) -> (CGImage?, String?) {
    guard
        let dataProvider = CGDataProvider(
            dataInfo: nil,
            data: UnsafeMutableRawPointer(mutating: imageData),
            size: Int(bytesPerLine * height),
            releaseData: { _, _, _ in }
        )
    else {
        return (nil, "Error making CGDataProvider")
    }

    guard let colorSpace = CGColorSpace(name: CGColorSpace.sRGB) else {
        return (nil, "Error making CGColorSpace")
    }

    let bitsPerComponent = 8
    let bitsPerPixel = 24

    guard
        let cgImage = CGImage(
            width: Int(width),
            height: Int(height),
            bitsPerComponent: bitsPerComponent,
            bitsPerPixel: bitsPerPixel,
            bytesPerRow: Int(bytesPerLine),
            space: colorSpace,
            bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.none.rawValue),
            provider: dataProvider,
            decode: nil,
            shouldInterpolate: false,
            intent: .defaultIntent
        )
    else {
        return (nil, "Error making CGImage")
    }

    return (cgImage, nil)
}

typealias TextRecognitionCallback = @convention(c) (
    UInt32 /* frame number */,
    UInt32 /* count */,
    UnsafeMutablePointer<UnsafeMutablePointer<CChar>?>? /* texts */,
    UnsafePointer<UInt32>? /* lefts */,
    UnsafePointer<UInt32>? /* tops */,
    UnsafePointer<UInt32>? /* rights */,
    UnsafePointer<UInt32>? /* bottoms */,
    UnsafePointer<Float>? /* confidences */,
    UnsafePointer<CChar>? /* optional error message */,
    UnsafeMutableRawPointer? /* userData */
) -> Void

@_cdecl("vk_free_ocr_results")
func freeResults(
    count: UInt32,
    texts: UnsafeMutablePointer<UnsafeMutablePointer<CChar>?>?,
    lefts: UnsafeMutablePointer<UInt32>?,
    tops: UnsafeMutablePointer<UInt32>?,
    rights: UnsafeMutablePointer<UInt32>?,
    bottoms: UnsafeMutablePointer<UInt32>?,
    confidences: UnsafeMutablePointer<Float>?,
    error: UnsafeMutablePointer<CChar>?
) {
    if let texts = texts {
        for i in 0..<Int(count) {
            if let text = texts[i] {
                free(text)
            }
        }
        texts.deallocate()
    }

    lefts?.deallocate()
    tops?.deallocate()
    rights?.deallocate()
    bottoms?.deallocate()
    confidences?.deallocate()

    if let error = error {
        free(error)
    }
}

@_cdecl("vk_do_ocr")
func recognizeTextInImage(
    frameNumber: UInt32,
    languages: UnsafePointer<CChar>?,
    imageData: UnsafePointer<UInt8>,
    width: UInt32,
    height: UInt32,
    bytesPerLine: UInt32,
    callback: @escaping TextRecognitionCallback,
    userData: UnsafeMutableRawPointer?
) {
    func sendError(_ message: String) {
        let errorPtr = strdup(message)
        callback(frameNumber, 0, nil, nil, nil, nil, nil, nil, errorPtr, userData)
    }

    // Split the language list
    guard let languages = languages else {
        sendError("Missing languages string")
        return
    }

    guard let languagesStr = String(cString: languages, encoding: .utf8) else {
        sendError("Error splitting languages string")
        return
    }

    let languageList = languagesStr.split(separator: "+").map { String($0) }

    // Load the image
    let (cgImage, error) = createCGImage(
        from: imageData,
        width: width,
        height: height,
        bytesPerLine: bytesPerLine
    )
    if let error = error {
        sendError(error)
        return
    }

    guard let image = cgImage else {
        sendError("Error loading image")
        return
    }

    #if false
        saveCGImage(image, "./img.png")
    #endif

    let requestHandler = VNImageRequestHandler(cgImage: image, options: [:])

    let request = VNRecognizeTextRequest { request, error in
        if let error = error {
            let errorString = "Vision error: \(error.localizedDescription)"
            sendError(errorString)
            return
        }

        guard let observations = request.results as? [VNRecognizedTextObservation] else {
            // Empty recognition result - not an error
            callback(frameNumber, 0, nil, nil, nil, nil, nil, nil, nil, userData)
            return
        }

        let cgWidth = CGFloat(width)
        let cgHeight = CGFloat(height)
        var recognitionEntries: [RecognitionEntry] = []

        for observation in observations {
            let entries = RecognitionEntry.from(observation, cgWidth, cgHeight)
            recognitionEntries += entries
        }

        let count = recognitionEntries.count
        guard count > 0 else {
            // Empty recognition result - not an error
            callback(frameNumber, 0, nil, nil, nil, nil, nil, nil, nil, userData)
            return
        }

        let texts = UnsafeMutablePointer<UnsafeMutablePointer<CChar>?>.allocate(capacity: count)
        let lefts = UnsafeMutablePointer<UInt32>.allocate(capacity: count)
        let tops = UnsafeMutablePointer<UInt32>.allocate(capacity: count)
        let rights = UnsafeMutablePointer<UInt32>.allocate(capacity: count)
        let bottoms = UnsafeMutablePointer<UInt32>.allocate(capacity: count)
        let confidences = UnsafeMutablePointer<Float>.allocate(capacity: count)

        var index = 0
        for entry in recognitionEntries {
            texts[index] = strdup(entry.text)
            lefts[index] = entry.left
            rights[index] = entry.right
            tops[index] = entry.top
            bottoms[index] = entry.bottom
            confidences[index] = entry.confidence
            index += 1
        }

        callback(frameNumber, UInt32(count), texts, lefts, tops, rights, bottoms, confidences, nil, userData)
    }

    request.recognitionLanguages = languageList
    request.recognitionLevel = .accurate
    request.minimumTextHeight = 8.0 / Float(height)
    request.usesLanguageCorrection = true

    do {
        try requestHandler.perform([request])
    } catch {
        sendError("VisionKit error: \(error)")
    }
}
