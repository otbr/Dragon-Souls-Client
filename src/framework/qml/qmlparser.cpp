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

#include "qmlparser.h"
#include <framework/otml/otmlexception.h>

OTMLDocumentPtr QMLParser::parse(std::istream& in, const std::string& source)
{
    QMLParser parser(in, source);
    auto doc = OTMLDocument::create();
    parser.parseNode(doc);
    return doc;
}

QMLParser::QMLParser(std::istream& in, const std::string& source) : m_in(in), m_source(source)
{
}

void QMLParser::skipWhitespace()
{
    char next;
    while (m_in.get(next)) {
        if (next == '\n') {
            m_line++;
        } else if (isspace(next)) {
            continue;
        } else if (next == '/') {
            if (m_in.peek() == '/') { // Single line comment
                while (m_in.get(next) && next != '\n');
                m_line++;
            } else if (m_in.peek() == '*') { // Multi line comment
                m_in.get(next); // consume '*'
                while (m_in.get(next)) {
                    if (next == '\n') m_line++;
                    if (next == '*' && m_in.peek() == '/') {
                        m_in.get(next); // consume '/'
                        break;
                    }
                }
            } else {
                m_in.unget();
                break;
            }
        } else {
            m_in.unget();
            break;
        }
    }
}

std::string QMLParser::parseValue()
{
    std::string value;
    char next;
    bool inQuote = false;
    int braceDepth = 0;
    int parenDepth = 0;
    int bracketDepth = 0;

    // remove leading spaces from value
    while (m_in.peek() == ' ' || m_in.peek() == '\t')
        m_in.get(next);

    while (m_in.get(next)) {
        if (next == '\n') {
            m_line++;
            if (!inQuote && braceDepth == 0 && parenDepth == 0 && bracketDepth == 0)
                break; // End of value on newline
        }

        if (next == '"') {
            inQuote = !inQuote;
        }

        if (!inQuote) {
            if (next == '{') braceDepth++;
            else if (next == '}') {
                if (braceDepth > 0) braceDepth--;
                else {
                    m_in.unget();
                    break;
                }
            } else if (next == '(') parenDepth++;
            else if (next == ')') parenDepth--;
            else if (next == '[') bracketDepth++;
            else if (next == ']') bracketDepth--;
            else if (next == ';') {
                if (braceDepth == 0 && parenDepth == 0 && bracketDepth == 0)
                    break;
            }
        }

        value += next;
    }

    // Trim trailing whitespace
    while (!value.empty() && isspace(value.back()))
        value.pop_back();

    // If exact quoted string, strip quotes
    if (value.length() >= 2 && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.length() - 2);
    }

    return value;
}

void QMLParser::parseNode(const OTMLNodePtr& parentNode)
{
    std::string identifier;
    char next;

    while (true) {
        skipWhitespace();
        if (m_in.eof()) break;

        identifier.clear();

        // Check for end of block
        if (m_in.peek() == '}') {
            m_in.get(next);
            return;
        }

        // Read identifier
        while (m_in.get(next)) {
            if (isalnum(next) || next == '_' || next == '.' || next == '-') {
                identifier += next;
            } else {
                m_in.unget();
                break;
            }
        }

        if (identifier.empty()) {
            if (m_in.peek() != '}')
                break;
            continue;
        }

        if (identifier == "import") {
            // consume until newline
            while (m_in.get(next) && next != '\n');
            m_line++;
            continue;
        }

        if (identifier == "signal") {
            // consume signal definition until newline or semicolon
            while (m_in.get(next)) {
                if (next == '\n') { m_line++; break; }
                if (next == ';') break;
            }
            continue;
        }

        if (identifier == "property") {
            // property <type/alias> <name> : <value>
            while (m_in.peek() == ' ' || m_in.peek() == '\t') m_in.get(next);
            std::string typeOrAlias;
            while (m_in.get(next)) {
                if (isspace(next)) break;
                typeOrAlias += next;
            }

            // read name
            while (m_in.peek() == ' ' || m_in.peek() == '\t') m_in.get(next);
            std::string propName;
            while (m_in.get(next)) {
                if (next == ':' || isspace(next)) { m_in.unget(); break; }
                propName += next;
            }

            skipWhitespace();
            if (m_in.peek() == ':') {
                m_in.get(next); // consume ':'
                std::string value = parseValue();  
                auto node = OTMLNode::create(propName);
                node->setValue(value);
                node->setUnique(true);
                node->setSource(stdext::format("%s:%d", m_source, m_line));
                parentNode->addChild(node);

                skipWhitespace();
                if (m_in.peek() == ';') m_in.get(next);
            }
            continue;
        }

        if (identifier == "function") {
            // function name() { ... }
            skipWhitespace();
            std::string funcName;
            while (m_in.get(next)) {
                if (next == '(' || isspace(next)) { m_in.unget(); break; }
                funcName += next;
            }
            
            // Treat the rest (args + body) as value
            std::string funcBody = parseValue();

            auto node = OTMLNode::create(funcName);
            node->setValue("function " + funcBody);
            node->setUnique(true);
            node->setSource(stdext::format("%s:%d", m_source, m_line));
            parentNode->addChild(node);
            continue;
        }

        skipWhitespace();

        if (m_in.peek() == ':') {
            // It's a property: id: value
            m_in.get(next); // consume ':'

            std::string value = parseValue();

            auto node = OTMLNode::create(identifier);
            node->setValue(value);
            node->setUnique(true);
            node->setSource(stdext::format("%s:%d", m_source, m_line));
            parentNode->addChild(node);

            // Optional semicolon
            skipWhitespace();
            if (m_in.peek() == ';') m_in.get(next);

        } else if (m_in.peek() == '{') {
            // It's a child definition: Identifier { ... }
            m_in.get(next); // consume '{'

            auto node = OTMLNode::create(identifier);
            node->setSource(stdext::format("%s:%d", m_source, m_line));
            parentNode->addChild(node);

            parseNode(node); // recurse

        } else {
            // unexpected
            throw OTMLException(parentNode, stdext::format("Unexpected character '%c' after identifier '%s' at line %d", (char)m_in.peek(), identifier, m_line));
        }
    }
}
