-- SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom
-- Copyright (C) 2020 Kernkonzept GmbH.
-- Author: Philipp Eppelt <philipp.eppelt@kernkonzept.com>
--
--
-- Usage note:
-- If you use this library you must abort further program startup if any of the
-- used functions return false.
-- In this case the library already printed a TAP TEST START - TAP TEST FINISH
-- block, which a) can lead to program termination from the outside, b)
-- further TAP blocks will be ignored by the infrastructure, c) additional
-- messages are likely not picked up by the infrastructure.
--
-- Errors during usage of this library will print a TAP block containing a
-- 'not ok' line with the error message.
-- A failure of a HWconfig check will print a TAP block containing an 'ok-skip'
-- line with the failed check.

local t = require("rom/test_env")
local conf = t.FULLCONFIG

local function print_tap_start()
  print "TAP TEST START"
end

local function print_tap_finish()
  print "TAP TEST FINISH"
end

local function print_tap_skip_line(reason)
  skip_msg = "1..0 # SKIP"
  if type(reason) == "string" then
    skip_msg = skip_msg .. " - " .. reason
  end

  print(skip_msg)
end

-- Print a failure message in TAP format. Use this instead of error().
--
-- error() terminates the program and leaves the TAP infrastructure waiting
-- until the timeout hits. Instead of waiting, we print a failure message in
-- TAP format, such that other tests can progress and the failure is recorded.
local function print_tap_failure(reason)
  local failure_msg = "1..1\nnot ok lib-test-setup - "
  if type(reason) == "string" then
    failure_msg = failure_msg .. reason
  end

  print_tap_start()
  print(failure_msg)
  print_tap_finish()
end

-- print the reason to skip the test in TAP format
local function print_tap_skip(msg)
  print_tap_start()
  print_tap_skip_line(msg)
  print_tap_finish()
end

-- Check for HWconfig and print default TAP messages if not.
local function conf_present()
  if conf == nil then
    print_tap_skip("Hardware configuration unknown")
    return false
  end

  return true
end

local function conf_prop(prop, val)
  if type(prop) ~= "string" or type(val) ~= "boolean" then
    print_tap_failure("conf_prop: Property must be of type string.")
    return false
  end

  -- convention with HWconfig: every property is upper case
  prop = string.upper(prop)

  if val and conf[prop] ~= "y" then
    print_tap_skip("Hardware does not support property " .. prop)
    return false
  elseif not val and conf[prop] == "y" then
    print_tap_skip("Hardware supports property " .. prop)
    return false;
  end

  return true
end

-- A property `prop` is set to "y".
-- pre: present() == true
-- param: prop = string
local function conf_prop_set(prop)
  return conf_prop(prop, true)
end

-- A property `prop` is set to "n".
-- pre: present() == true
-- param: prop = string
local function conf_prop_unset(prop)
  return conf_prop(prop, false)
end

-- All of the passed properties are set to "y".
-- pre: present() == true
-- param: ... = string
local function conf_props_set(...)
  for _, prop in pairs({...}) do
    if not conf_prop_set(prop) then
      return false
    end
  end

  return true
end

-- All of the passed properties are set to "n".
-- pre: present() == true
-- param: ... = string
local function conf_props_unset(...)
  for _, prop in pairs({...}) do
    if not conf_prop_unset(prop) then
      return false
    end
  end

  return true
end

-- A property `prop` compares to value used in `prop_test_fn`.
-- pre: present() == true
-- param: prop = string
-- param: prop_test_fn = function testing the property
--
-- returns: comparison result and error_msg.
local function prop_val_cmp(prop, prop_test_fn)
  if type(prop) ~= "string" then
    print_tap_failure("prop_val_cmp: Property must be of type string.")
    return false
  end
  if type(prop_test_fn) ~= "function" then
    print_tap_failure("prop_val_cmp: Compare function must be of type function.")
    return false
  end

  -- convention with HWconfig: every property is upper case
  prop = string.upper(prop)

  if (conf[prop] == nil) then
    print_tap_skip("Hardware does not feature property " .. prop .. ".")
    return false
  end

  if not prop_test_fn(conf[prop]) then
    print_tap_skip("Hardware provides " .. prop .. " = " .. conf[prop] .. ". "
                   ..  "This does not satisfy the requested property value.")
    return false
  end

  return true
end

-- A property `prop` compares to value used in `prop_test_fn`.
-- pre: present() == true
-- param: prop = string
-- param: prop_test_fn = function testing the property
--
-- usage:
--  ret = conf_prop_val_cmp(num_cpus, function (v1) return v1 > 1 end)
local function conf_prop_val_cmp(prop, prop_test_fn)
  return prop_val_cmp(prop, prop_test_fn)
end

-- All of the passed property-comparator-pairs are satisfied.
--
-- usage:
-- conf_props_values_cmp({ ["num_cpus"] = function (v) return v == 1 end,
--                         ["FOO"] = prop_test_fnB })
local function conf_props_values_cmp(...)
  local _, tbl = pairs({...})
  if type(tbl) ~= "table" then
    print_tap_failure("conf_props_values_cmp: Parameter must be of type table.")
    return false
  end

  for prop, fn in pairs(tbl) do
    if not prop_val_cmp(prop, fn) then
      return false
    end
  end

  return true
end

return {
  print_tap_start = print_tap_start,
  print_tap_finish = print_tap_start,
  print_tap_skip_line = print_tap_skip_line,
  print_tap_skip = print_tap_skip,
  conf_prop_set = conf_prop_set,
  conf_prop_unset = conf_prop_unset,
  conf_props_set = conf_props_set,
  conf_props_unset = conf_props_unset,
  conf_present = conf_present,
  conf_prop_val = conf_prop_val_cmp,
  conf_props_vals = conf_props_values_cmp
}
