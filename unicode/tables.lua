local TableParser = {}
function TableParser.New(docURL, inPath, outPath)
    return setmetatable({
        _DocURL = docURL,
        _InPath = inPath,
        _OutPath = outPath,
        _DocComments = {},
        _Ranges = {}
    }, {
        __index = TableParser
    })
end

function TableParser:_ParseComment(line, storeComments)
    return false
end

function TableParser:_ParseField(line, storeComments)
    return false
end

function TableParser:GetURL()
    return self._DocURL or self._InPath
end

function TableParser:Parse(storeComments)
    local inFile = io.open(self._InPath, "r")
    if not inFile then
        error("Please download '" .. self:GetURL() .. "' before running this script.")
    end

    for line in inFile:lines() do
        if #line > 0 and not self:_ParseComment(line, storeComments) then
            if not self:_ParseField(line, storeComments) then
                error("LINE NOT PARSED: " .. line)
            end
        end
    end
    inFile:close()
end

-- Joins overlapping ranges
function TableParser:Optimize()
    local ranges = {self._Ranges[1]}
    local lastInsertedRange = ranges[1]

    for i=2, #self._Ranges do
        local range = self._Ranges[i]
        if tonumber(lastInsertedRange[2], 16)+1 >= tonumber(range[1], 16) then
            lastInsertedRange[2] = range[2]
            for _, comment in next, range[3] do
                table.insert(lastInsertedRange[3], comment)
            end
        else
            table.insert(ranges, range)
            lastInsertedRange = range
        end
    end

    self._Ranges = ranges
end

-- Asserts that self._Ranges is actually sorted in ascending order
function TableParser:AssertRanges()
    local rangesCount = #self._Ranges
    if rangesCount <= 0 then return false; end

    local prevRange = self._Ranges[1]
    assert(
        tonumber(prevRange[1], 16) <= tonumber(prevRange[2], 16),
        prevRange[1] .. " > " .. prevRange[2])

    for i=2, rangesCount do
        local range = self._Ranges[i]
        assert(
            tonumber(range[1], 16) <= tonumber(range[2], 16),
            range[1] .. " > " .. range[2])
        assert(
            tonumber(prevRange[2], 16) < tonumber(range[1], 16),
            prevRange[1] .. " >= " .. range[1])
        prevRange = range
    end
end

function TableParser:_WriteHead(outFile)
    error("_WriteHead not implemented")
end

function TableParser:_WriteBody(outFile)
    for _, comment in next, self._DocComments do
        outFile:write(table.concat{"//", comment, "\n"})
    end
    for _, range in next, self._Ranges do
        local commentsCount = #range[3]
        if commentsCount > 1 then
            for _, comment in next, range[3] do
                outFile:write(table.concat{"    //", comment, "\n"})
            end
        end

        outFile:write(table.concat{
            "    { 0x", range[1], ", 0x", range[2], " },",
            commentsCount == 1 and (" //" .. range[3][1]) or "",
            "\n"
        })
    end
end

function TableParser:_WriteTail(outFile)
    error("_WriteTail not implemented")
end

function TableParser:Save()
    local outFile = io.open(self._OutPath, "w")
    if not outFile then
        error("Could not write to '" .. self._InPath .. "'.")
    end

    self:_WriteHead(outFile)
    self:_WriteBody(outFile)
    self:_WriteTail(outFile)

    outFile:close()
end

--== EAST ASIAN WIDTH TABLE PARSER ==--

local EastAsianWidthTableParser = setmetatable({}, { __index = TableParser })
EastAsianWidthTableParser.EXPORTED_WIDTHS = { ["F"] = true, ["W"] = true }

function EastAsianWidthTableParser.New(docURL, inPath, outPath)
    return setmetatable(TableParser.New(docURL, inPath, outPath), {
        __index = EastAsianWidthTableParser
    })
end

function EastAsianWidthTableParser:_ParseComment(line, storeComments)
    local comment = line:match("^%s*#(.*)")
    if not comment then return false; end
    if storeComments then
        table.insert(self._DocComments, comment)
    end
    return true
end

function EastAsianWidthTableParser:_ParseField(line, storeComments)
    local a, b
    local code, width, comment = line:match("^%s*(%x+)%s*;%s*(%a+)%s*#(.*)")

    if code then
        a = code
        b = code
    else
        a, b, width, comment = line:match("^%s*(%x+)%.%.(%x+)%s*;%s*(%a+)%s*#(.*)")
        if not a then return false; end
    end

    if not self.EXPORTED_WIDTHS[width] then
        return true
    end

    if storeComments then
        table.insert(self._Ranges, {a, b, {comment}})
    else
        table.insert(self._Ranges, {a, b, {}})
    end

    return true
end

function EastAsianWidthTableParser:_WriteHead(outFile)
    local rangesCount = #self._Ranges
    outFile:write(table.concat{"constexpr size_t EastAsianWideCharacters_Count = ", tostring(rangesCount), ";\n"})

    outFile:write("// " .. self:GetURL() .. "\n")
    local exportedWidths = {}
    for t, _ in next, self.EXPORTED_WIDTHS do
        table.insert(exportedWidths, t)
    end
    outFile:write("// Exported widths: " .. table.concat(exportedWidths, ", ") .. "\n")

    outFile:write("constexpr Range EastAsianWideCharacters[EastAsianWideCharacters_Count] = {\n")
end

function EastAsianWidthTableParser:_WriteTail(outFile)
    outFile:write("};\n")
end

--== EMOJI SEQUENCES TABLE PARSER ==--

local EmojiSequencesTableParser = setmetatable({}, { __index = TableParser })
EmojiSequencesTableParser.EXPORTED_TYPES = { ["Basic_Emoji"] = true }

function EmojiSequencesTableParser.New(docURL, inPath, outPath)
    return setmetatable(TableParser.New(docURL, inPath, outPath), {
        __index = EmojiSequencesTableParser
    })
end

function EmojiSequencesTableParser:_ParseComment(line, storeComments)
    local comment = line:match("^%s*#(.*)")
    if not comment then return false; end
    if storeComments then
        table.insert(self._DocComments, comment)
    end
    return true
end

function EmojiSequencesTableParser:_ParseField(line, storeComments)
    local codePoints, emojiType, comment = line:match("^([^;]+);%s*([%a_]+)%s*;[^#]+#(.*)")
    if not codePoints then return false; end

    if not self.EXPORTED_TYPES[emojiType] then return true; end

    local a, b
    local code = codePoints:match("^%s*(%x+)%s*$")
    if code then
        a = code
        b = code
    else
        a, b = codePoints:match("^%s*(%x+)%.%.(%x+)%s*$")
        if not a then
            -- At least make sure we know it's a sequence
            -- If not then an error will be raised and the
            --  user will be notified.
            if codePoints:match("^%s*%x+%s+%x+%s*$") then
                return true
            else
                return false
            end
        end
    end

    if storeComments then
        table.insert(self._Ranges, {a, b, {comment}})
    else
        table.insert(self._Ranges, {a, b, {}})
    end
    return true
end

function EmojiSequencesTableParser:_WriteHead(outFile)
    local rangesCount = #self._Ranges
    outFile:write(table.concat{"constexpr size_t EmojiCharacters_Count = ", tostring(rangesCount), ";\n"})

    outFile:write("// " .. self:GetURL() .. "\n")
    local exportedTypes = {}
    for t, _ in next, self.EXPORTED_TYPES do
        table.insert(exportedTypes, t)
    end
    outFile:write("// Exported types: " .. table.concat(exportedTypes, ", ") .. "\n")

    outFile:write("constexpr Range EmojiCharacters[EmojiCharacters_Count] = {\n")
end

function EmojiSequencesTableParser:_WriteTail(outFile)
    outFile:write("};\n")
end

--== MAIN ==--

-- Call the script with argument "store" if you want to keep comments

local function main(storeComments)
    local tables = {
        EastAsianWidthTableParser.New(
            "https://unicode.org/Public/16.0.0/ucd/EastAsianWidth.txt",
            "EastAsianWidth.txt",
            "EastAsianWidth.cpp"
        ),
        EmojiSequencesTableParser.New(
            "https://unicode.org/Public/emoji/16.0/emoji-sequences.txt",
            "emoji-sequences.txt",
            "emoji-sequences.cpp"
        )
    }

    local doStoreComments = storeComments == "store"
    for _, t in next, tables do
        print("Parsing", t:GetURL())
        t:Parse(doStoreComments)

        print("Optimizing")
        t:Optimize()

        print("Verifying Order Range")
        t:AssertRanges()

        print("Saving Table")
        t:Save()

        print()
    end
end

main(...)
