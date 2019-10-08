-- ----------------------------------------------------------------------------
-- test.lua
-- Luabins test suite
-- See copyright notice in luabins.h
-- ----------------------------------------------------------------------------

package.cpath = "./?.so;"..package.cpath

local pack = pack or table.pack
local unpack = unpack or table.unpack
local randomseed = 1235134892
--local randomseed = os.time()

print("===== BEGIN LUABINS TEST SUITE (seed " .. randomseed .. ") =====")
math.randomseed(randomseed)

-- ----------------------------------------------------------------------------
-- Utility functions
-- ----------------------------------------------------------------------------

local invariant = function(v)
  return function()
    return v
  end
end

local escape_string = function(str)
  return str:gsub(
      "[^0-9A-Za-z_%- :]",
      function(c)
        return ("%%%02X"):format(c:byte())
      end
    )
end

local ensure_equals = function(msg, actual, expected)
  if actual ~= expected then
    error(
        msg..":\n  actual: `"..escape_string(tostring(actual))
        .."`\nexpected: `"..escape_string(tostring(expected)).."'"
      )
  end
end

local ensure_equals_permute
do
  -- Based on MIT-licensed
  -- http://snippets.luacode.org/sputnik.lua?p=snippets/ \
  -- Iterator_over_Permutations_of_a_Table_62
  -- Which is based on PiL
  local function permgen(a, n, fn)
    if n == 0 then
      fn(a)
    else
      for i = 1, n do
        -- put i-th element as the last one
        a[n], a[i] = a[i], a[n]

        -- generate all permutations of the other elements
        permgen(a, n - 1, fn)

        -- restore i-th element
        a[n], a[i] = a[i], a[n]
      end
    end
  end

  --- an iterator over all permutations of the elements of a list.
  -- Please note that the same list is returned each time,
  -- so do not keep references!
  -- @param a list-like table
  -- @return an iterator which provides the next permutation as a list
  local function permute_iter(a, n)
    local n = n or #a
    local co = coroutine.create(function() permgen(a, n, coroutine.yield) end)
    return function() -- iterator
      local code, res = coroutine.resume(co)
      return res
    end
  end

  ensure_equals_permute = function(
      msg,
      actual,
      expected_prefix,
      expected_body,
      expected_suffix,
      expected_body_size
    )
    expected_body_size = expected_body_size or #expected_body

    local expected
    for t in permute_iter(expected_body, expected_body_size) do
      expected = expected_prefix .. table.concat(t) .. expected_suffix
      if actual == expected then
        return actual
      end
    end

    error(
        msg..":\nactual: `"..escape_string(tostring(actual))
        .."`\nexpected one of permutations: `"
        ..escape_string(tostring(expected)).."'"
      )
  end
end

local function deepequals(lhs, rhs)
  if type(lhs) ~= "table" or type(rhs) ~= "table" then
    return lhs == rhs
  end

  local checked_keys = {}

  for k, v in pairs(lhs) do
    checked_keys[k] = true
    if not deepequals(v, rhs[k]) then
      return false
    end
  end

  for k, v in pairs(rhs) do
    if not checked_keys[k] then
      return false -- extra key
    end
  end

  return true
end

local nargs = function(...)
  return select("#", ...), ...
end

local pack = function(...)
  return select("#", ...), { ... }
end

local eat_true = function(t, ...)
  if t == nil then
    error("failed: " .. (...))
  end
  return ...
end

-- ----------------------------------------------------------------------------
-- Test helper functions
-- ----------------------------------------------------------------------------

luabins = require 'luabins'
local luabins_local = require 'luabins'
assert(luabins_local == luabins)

assert(type(luabins.save) == "function")
assert(type(luabins.load) == "function")

local check_load_fn_ok = function(eq, saved, ...)
  local expected = { nargs(...) }
  local loaded = { nargs(eat_true(luabins.load(saved))) }

  ensure_equals("num arguments match", loaded[1], expected[1])
  for i = 2, expected[1] do
    assert(eq(loaded[i], expected[i]))
  end

  return saved
end

local check_load_ok = function(saved, ...)
  return check_load_fn_ok(deepequals, saved, ...)
end

local check_fn_ok = function(eq, ...)
  local saved = assert(luabins.save(...))

  assert(type(saved) == "string")

  print("saved length", #saved, "(display truncated to 70 chars)")
  print(escape_string(saved):sub(1, 70))

  return check_load_fn_ok(eq, saved, ...)
end

local check_ok = function(...)
  print("check_ok")
  return check_fn_ok(deepequals, ...)
end

local check_fail_save = function(msg, ...)
  print("check_fail_save")
  local res, err = luabins.save(...)
  ensure_equals("result", res, nil)
  ensure_equals("error message", err, msg)
--  print("/check_fail_save")
end

local check_fail_load = function(msg, v)
  print("check_fail_load")
  local res, err = luabins.load(v)
  ensure_equals("result", res, nil)
  ensure_equals("error message", err, msg)
--  print("/check_fail_load")
end

print("===== BEGIN LARGE DATA OK =====")

-- Based on actual bug.
-- This dataset triggered Lua C data stack overflow.
-- (Note that bug is not triggered if check_ok is used)
-- Update data with
-- $ lua etc/toluabins.lua test/large_data.lua>test/large_data.luabins
-- WARNING: Keep this test above other tests, so Lua stack is small.
assert(
    luabins.load(
        assert(io.open("test/large_data.luabins", "r"):read("*a"))
      )
  )

print("===== LARGE DATA OK =====")

-- ----------------------------------------------------------------------------
-- Basic tests
-- ----------------------------------------------------------------------------

print("===== BEGIN BASIC TESTS =====")

print("---> basic corrupt data tests")

check_fail_load("can't load: corrupt data", "")
check_fail_load("can't load: corrupt data", "bad data")

print("---> basic extra data tests")
do
  local s

  s = check_ok()
  check_fail_load("can't load: extra data at end", s .. "-")

  s = check_ok(nil)
  check_fail_load("can't load: extra data at end", s .. "-")

  s = check_ok(true)
  check_fail_load("can't load: extra data at end", s .. "-")

  s = check_ok(false)
  check_fail_load("can't load: extra data at end", s .. "-")

  s = check_ok(42)
  check_fail_load("can't load: extra data at end", s .. "-")

  s = check_ok(math.pi)
  check_fail_load("can't load: extra data at end", s .. "-")

  s = check_ok(1/0)
  check_fail_load("can't load: extra data at end", s .. "-")

  s = check_ok(-1/0)
  check_fail_load("can't load: extra data at end", s .. "-")

  s = check_ok("Luabins")
  check_fail_load("can't load: extra data at end", s .. "-")

  s = check_ok({ })

  check_fail_load("can't load: extra data at end", s .. "-")

  s = check_ok({ a = 1, 2 })
  check_fail_load("can't load: extra data at end", s .. "-")
end

print("---> basic type tests")

-- This is the way to detect NaN
check_fn_ok(function(lhs, rhs) return lhs ~= lhs and rhs ~= rhs end, 0/0)

check_ok("")

check_ok("Embedded\0Zero")

check_ok(("longstring"):rep(1024000))

check_fail_save("can't save: unsupported type detected", function() end)
check_fail_save(
    "can't save: unsupported type detected",
    coroutine.create(function() end)
  )
check_fail_save("can't save: unsupported type detected", function()end)

print("---> basic table tests")

check_ok({ 1 })
check_ok({ a = 1 })
check_ok({ a = 1, 2, [42] = true, [math.pi] = math.huge })
check_ok({ { } })
check_ok({ a = {}, b = { c = 7 } })
check_ok({ 1, 2, 3 })
check_ok({ [1] = 1, [1.5] = 2, [2] = 3 })
check_ok({ 1, nil, 3 })
check_ok({ 1, nil, 3, [{ 1, nil, 3 }] = { 1, nil, 3 } })

print("---> basic tuple tests")

check_ok(nil, nil)

do
  local s = check_ok(nil, false, true, 42, "Embedded\0Zero", { { [{3}] = 54 } })
  check_fail_load("can't load: extra data at end", s .. "-")

  check_ok(check_ok(s)) -- Save data string couple of times more
end

print("---> basic table tuple tests")

check_ok({ a = {}, b = { c = 7 } }, nil, { { } }, 42)

check_ok({ ["1"] = "str", [1] = "num" })

check_ok({ [true] = true })
check_ok({ [true] = true, [false] = false, 1 })

print("---> basic fail save tests")

check_fail_save(
    "can't save: unsupported type detected",
    { { function() end } }
  )

check_fail_save(
    "can't save: unsupported type detected",
    nil, false, true, 42, "Embedded\0Zero", function() end,
    { { [{3}] = 54 } }
  )

print("---> recursive table test")

local t = {}; t[1] = t
check_fail_save("can't save: nesting is too deep", t)

print("---> metatable test")

check_ok(setmetatable({}, {__index = function(t, k) return k end}))

print("===== BASIC TESTS OK =====")

print("===== BEGIN FORMAT SANITY TESTS =====")

-- Format sanity checks for LJ2 compatibility tests.
-- These tests are intended to help debugging actual problems
-- of test suite, and are not feature complete.
-- What is not checked here, checked in the rest of suite.

do
  do
    local saved = check_ok(1)
    local expected =
      "\001".."N"
      .. "\000\000\000\000\000\000\240\063" -- Note number is a double

    ensure_equals(
        "1 as number",
        expected,
        saved
      )
  end

  do
    local saved = check_ok({ [true] = 1 })
    local expected =
      "\001".."T"
      .. "\000\000\000\000".."\001\000\000\000"
      .. "1"
      .. "N\000\000\000\000\000\000\240\063" -- Note number is a double

    ensure_equals(
        "1 as value",
        expected,
        saved
      )
  end

  do
    local saved = check_ok({ [1] = true })
    local expected =
      "\001".."T"
      .. "\001\000\000\000".."\000\000\000\000"
      .. "N\000\000\000\000\000\000\240\063" -- Note number is a double
      .. "1"

    ensure_equals(
        "1 as key",
        expected,
        saved
      )
  end
end

print("===== FORMAT SANITY TESTS OK =====")

print("===== BEGIN AUTOCOLLAPSE TESTS =====")

-- Note: those are ad-hoc tests, tuned for old implementation
-- which generated save data on Lua stack.
-- These tests are kept here for performance comparisons.

local LUABINS_CONCATTHRESHOLD = 1024

local gen_t = function(size)
  -- two per numeric entry, three per string entry,
  -- two entries per key-value pair
  local actual_size = math.ceil(size / (2 + 3))
  print("generating table of "..actual_size.." pairs")
  local t = {}
  for i = 1, actual_size do
    t[i] = "a"..i
  end
  return t
end

-- Test table value autocollapse
check_ok(gen_t(LUABINS_CONCATTHRESHOLD - 100)) -- underflow, no autocollapse
check_ok(gen_t(LUABINS_CONCATTHRESHOLD)) -- autocollapse, no extra elements
check_ok(gen_t(LUABINS_CONCATTHRESHOLD + 100)) -- autocollapse, extra elements

-- Test table key autocollapse
check_ok({ [gen_t(LUABINS_CONCATTHRESHOLD - 4)] = true })

-- Test multiarg autocollapse
check_ok(
    1,
    gen_t(LUABINS_CONCATTHRESHOLD - 5),
    2,
    gen_t(LUABINS_CONCATTHRESHOLD - 5),
    3
  )

print("===== AUTOCOLLAPSE TESTS OK =====")

print("===== BEGIN MIN TABLE SIZE TESTS =====")

do
  -- one small key
  do
    local data = { [true] = true }
    local saved = check_ok(data)
    ensure_equals(
        "format sanity check",
        "\001".."T".."\000\000\000\000".."\001\000\000\000".."11",
        saved
      )
    check_fail_load(
        "can't load: corrupt data, bad size",
        saved:sub(1, #saved - 1)
      )

    -- As long as array and hash size sum is correct
    -- (and both are within limits), load is successful.
    -- If values are swapped, we get some performance hit.
    check_load_ok(
        "\001".."T".."\001\000\000\000".."\000\000\000\000".."11",
        data
      )

    check_fail_load(
        "can't load: corrupt data, bad size",
        "\001".."T".."\001\000\000\000".."\001\000\000\000".."11"
      )

    check_fail_load(
        "can't load: corrupt data, bad size",
        "\001".."T".."\000\000\000\000".."\002\000\000\000".."11"
      )

    check_fail_load(
        "can't load: extra data at end",
        "\001".."T".."\000\000\000\000".."\000\000\000\000".."11"
      )

    check_fail_load(
        "can't load: corrupt data, bad size",
        "\001".."T".."\255\255\255\255".."\255\255\255\255".."11"
      )
    check_fail_load(
        "can't load: corrupt data, bad size",
        "\001".."T".."\000\255\255\255".."\000\255\255\255".."11"
      )
    check_fail_load(
        "can't load: corrupt data, bad size",
        "\255".."T".."\000\000\000\000".."\000\000\000\000"
      )
  end

  -- two small keys
  do
    local data = { [true] = true, [false] = false }
    local saved = check_ok({ [true] = true, [false] = false })
    ensure_equals_permute(
        "format sanity check",
        saved,
        "\001" .. "T" .. "\000\000\000\000" .. "\002\000\000\000",
        {
          "0" .. "0";
          "1" .. "1";
        },
        ""
      )
    check_fail_load(
        "can't load: corrupt data, bad size",
        saved:sub(1, #saved - 1)
      )

    -- See above about swapped array and hash sizes
    check_load_ok(
        "\001".."T".."\001\000\000\000".."\001\000\000\000".."1100",
        data
      )

    check_fail_load(
        "can't load: corrupt data, bad size",
        "\001".."T".."\000\000\000\000".."\003\000\000\000".."0011"
      )
  end

  -- two small and one large key
  do
    local saved = check_ok({ [true] = true, [false] = false, [1] = true })
    ensure_equals_permute(
        "format sanity check",
        saved,
        "\001" .. "T" .. "\001\000\000\000" .. "\002\000\000\000",
        {
          "0" .. "0";
          "1" .. "1";
           -- Note number is a double
          "N\000\000\000\000\000\000\240\063" .. "1";
        },
        ""
      )

    check_fail_load(
        "can't load: corrupt data",
        saved:sub(1, #saved - 1)
      )

    check_fail_load(
        "can't load: corrupt data, bad size",
        "\001".."T"
        .. "\002\000\000\000".."\002\000\000\000"
        .. "0011"
        .. "N\000\000\000\000\000\000\240\063"
        .. "1"
      )

    check_fail_load(
        "can't load: corrupt data, bad size",
        "\001".."T"
        .. "\001\000\000\000".."\003\000\000\000"
        .. "0011"
        .. "N\000\000\000\000\000\000\240\063"
        .. "1"
      )
  end

  -- two small and two large keys
  do
    local saved = check_ok(
        { [true] = true, [false] = false, [1] = true, [42] = true }
      )
    local expected =
      "\001".."T"
      .. "\001\000\000\000".."\003\000\000\000"
      .. "0011"
    ensure_equals_permute(
        "format sanity check",
        saved,
        "\001" .. "T" .. "\001\000\000\000" .. "\003\000\000\000",
        {
          "0" .. "0";
          "1" .. "1";
          "N\000\000\000\000\000\000\069\064" .. "1";
          "N\000\000\000\000\000\000\240\063" .. "1";
        },
        ""
      )

    check_fail_load(
        "can't load: corrupt data",
        saved:sub(1, #saved - 1)
      )

    check_fail_load(
        "can't load: corrupt data, bad size",
        "\001".."T"
        .. "\001\000\000\000".."\005\000\000\000"
        .. "0011"
        .. "N\000\000\000\000\000\000\069\064"
        .. "1"
        .. "N\000\000\000\000\000\000\240\063"
        .. "1"
      )

    check_fail_load(
        "can't load: corrupt data, bad size",
        "\001".."T"
        .. "\003\000\000\000".."\003\000\000\000"
        .. "0011"
        .. "N\000\000\000\000\000\000\069\064"
        .. "1"
        .. "N\000\000\000\000\000\000\240\063"
        .. "1"
      )
  end
end

print("===== MIN TABLE SIZE TESTS OK =====")

print("===== BEGIN LOAD TRUNCATION TESTS =====")

local function gen_random_dataset(num, nesting)
  num = num or math.random(0, 128)
  nesting = nesting or 1

  local gen_str = function()
    local t = {}
    local n = math.random(0, 1024)
    for i = 1, n do
      t[i] = string.char(math.random(0, 255))
    end
    return table.concat(t)
  end

  local gen_bool = function() return math.random() >= 0.5 end

  local gen_nil = function() return nil end

  local generators =
  {
    gen_nil;
    gen_nil;
    gen_nil;
    gen_bool;
    gen_bool;
    gen_bool;
    function() return math.random() end;
    function() return math.random(-10000, 10000) end;
    function() return math.random() * math.random(-10000, 10000) end;
    gen_str;
    gen_str;
    gen_str;
    function()
      if nesting >= 24 then
        return nil
      end

      local t = {}
      local n = math.random(0, 24 - nesting)
      for i = 1, n do
        local k = gen_random_dataset(1, nesting + 1)
        if k == nil then
          k = "(nil)"
        end
        t[ k ] = gen_random_dataset(
            1,
            nesting + 1
          )
      end

      return t
    end;
  }

  local t = {}
  for i = 1, num do
    local n = math.random(1, #generators)
    t[i] = generators[n]()
  end
  return unpack(t, 0, num)
end

local random_dataset_num, random_dataset_data = pack(gen_random_dataset())
local random_dataset_saved = check_ok(
    unpack(random_dataset_data, 0, random_dataset_num)
  )

local num_tries = 100
local errors = {}
for i = 1, num_tries do
  local to = math.random(1, #random_dataset_saved - 1)
  local new_data = random_dataset_saved:sub(1, to)

  local res, err = luabins.load(new_data)
  ensure_equals("truncated data must not be loaded", res, nil)
  errors[err] = (errors[err] or 0) + 1
end

print("truncation errors encountered:")
for err, n in pairs(errors) do
  print(err, n)
end

print("===== BASIC LOAD TRUNCATION OK =====")

print("===== BEGIN LOAD MUTATION TESTS =====")

local function mutate_string(str, num, override)
  num = num or math.random(1, 8)

  if num < 1 then
    return str
  end

  local mutators =
  {
    -- truncate at end
    function(str)
      local pos = math.random(1, #str)
      return str:sub(1, pos)
    end;
    -- truncate at beginning
    function(str)
      local pos = math.random(1, #str)
      return str:sub(-pos)
    end;
    -- cut out the middle
    function(str)
      local from = math.random(1, #str)
      local to = math.random(from, #str)
      return str:sub(1, from) .. str:sub(to)
    end;
    -- swap two halves
    function(str)
      local pos = math.random(1, #str)
      return str:sub(pos + 1, #str) .. str:sub(1, pos)
    end;
    -- swap two characters
    function(str)
      local pa, pb = math.random(1, #str), math.random(1, #str)
      local a, b = str:sub(pa, pa), str:sub(pb, pb)
      return
        str:sub(1, pa - 1) ..
        a ..
        str:sub(pa + 1, pb - 1) ..
        b ..
        str:sub(pb + 1, #str)
    end;
    -- replace one character
    function(str)
      local pos = math.random(1, #str)
      return
        str:sub(1, pos - 1) ..
        string.char(math.random(0, 255)) ..
        str:sub(pos + 1, #str)
    end;
    -- increase one character
    function(str)
      local pos = math.random(1, #str)
      local b = str:byte(pos, pos) + 1
      if b > 255 then
        b = 0
      end
      return
        str:sub(1, pos - 1) ..
        string.char(b) ..
        str:sub(pos + 1, #str)
    end;
    -- decrease one character
    function(str)
      local pos = math.random(1, #str)
      local b = str:byte(pos, pos) - 1
      if b < 0 then
        b = 255
      end
      return
        str:sub(1, pos - 1) ..
        string.char(b) ..
        str:sub(pos + 1, #str)
    end;
  }

  local n = override or math.random(1, #mutators)

  str = mutators[n](str)

  return mutate_string(str, num - 1, override)
end

local num_tries = 100000
local num_successes = 0
local errors = {}
for i = 1, num_tries do
  local new_data = mutate_string(random_dataset_saved)

  local res, err = luabins.load(new_data)
  if res == nil then
    errors[err] = (errors[err] or 0) + 1
  else
     num_successes = num_successes + 1
  end
end

if num_successes == 0 then
  print("no mutated strings loaded successfully")
else
  -- This is ok, since we may corrupt data, not format.
  -- If it is an issue for user, he must append checksum to data,
  -- as usual.
  print("mutated strings loaded successfully: "..num_successes)
end

print("mutation errors encountered:")
for err, n in pairs(errors) do
  print(err, n)
end

print("===== BASIC LOAD MUTATION OK =====")

print("OK")
