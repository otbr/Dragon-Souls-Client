/*
 * Copyright (c) 2010-2025 OTClient <https://github.com/edubart/otclient>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qmldocument.h"
#include "qmlparser.h"
#include <framework/core/resourcemanager.h>

OTMLDocumentPtr QMLDocument::parse(const std::string& fileName)
{
    try {
        std::string buffer = g_resources.readFileContents(fileName);
        std::stringstream ss(buffer);
        return parse(ss, fileName);
    } catch (std::exception& e) {
        g_logger.error(stdext::format("Failed to parse QML file '%s': %s", fileName, e.what()));
        return nullptr;
    }
}

OTMLDocumentPtr QMLDocument::parse(std::istream& in, const std::string& source)
{
    auto doc = QMLParser::parse(in, source);
    if (!doc) return nullptr;

    // Helper to recursively normalize QML nodes to OTUI standards
    std::function<void(const OTMLNodePtr&)> normalize = [&](const OTMLNodePtr& node) {
        std::string tag = node->tag();
        
        // --- Component Mapping ---
        if (tag == "Item" || tag == "Rectangle") {
            node->setTag("UIWidget");
            
            // Map 'color' to 'background-color' for Rectangles/Items
            if (node->hasChildAt("color")) {
                auto colorNode = node->get("color");
                colorNode->setTag("background-color");
            }
        } 
        else if (tag == "Text" || tag == "Label") {
            node->setTag("UILabel");
        }
        else if (tag == "Image") {
            node->setTag("UIWidget");
            // Map 'source' to 'image-source'
            if (node->hasChildAt("source")) {
                auto sourceNode = node->get("source");
                sourceNode->setTag("image-source");
            }
        }
        else if (tag == "MouseArea") {
            node->setTag("UIWidget");
        }
        
        // Recurse
        for (const auto& child : node->children()) {
            // If the child is an object definition (starts with uppercase), recurse mapping
            if (!child->tag().empty() && std::isupper(child->tag()[0])) {
                normalize(child);
            }
        }
    };

    for (const auto& node : doc->children()) {
        normalize(node);
    }

    return doc;
}
