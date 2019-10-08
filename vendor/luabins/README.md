luabins — Lua Binary Serialization Library
==========================================

Allows to save tuples of primitive Lua types into binary chunks
and to load saved data back.

NB: You may be better off with luatexts: https://github.com/agladysh/luatexts.

On serialization
----------------

### Luabins works with

 *  `nil`
 *  `boolean`
 *  `number`
 *  `string`
 *  `table` (see below)

### Luabins refuses to save

 *  `function`
 *  `thread`
 *  `userdata`

Luabins intentionally does not save or check any meta-information
(versions, endianness etc.) along with data. If needed, it is to be handled
elsewhere.

### Table serialization

1.  Metatatables are ignored.
2.  Table nesting depth should be no more than `LUABINS_MAXTABLENESTING`.
3.  On table save references are not honored. Each encountered reference
    becomes independent object on load:

        local t = { 42 }
        { t, t }

    becomes

        { { 42 }, { 42 } }

    that is, three separate tables instead of two.

Lua API
-------

 *  `luabins.save(...)`

    Saves arguments into a binary string.

     *   On success returns that string.
     *   On failure returns nil and error message.

    Example:

        local str = assert(luabins.save(1, "two", { "three", 4 }))

 *  `luabins.load(string)`

    Loads a list of values from a binary string.

     *  On success returns true and loaded values.
     *  On failure returns nil and error message.

    Example:

    If you do not know in advance what data is stored inside a binary string,
    you may put results into a table:

        local values = { luabins.load(data) }
        assert(values[1], values[2])

    If you know how to handle stored values (for example you're sure they were
    generated following some established protocol), you may want to use
    something like this function to check `luabins.load()` result:

        function eat_true(t, ...)
            assert(t, ...)
            return ...
        end

        my_value_handler(eat_true(luabins.load(data)))

C API
-----

 * `int luabins_save(lua_State * L, int index_from, int index_to)`

    Save Lua values from given state at given stack index range.
    Lua value is left untouched. Note that empty range is not an error.
    You may save from 0 to `LUABINS_MAXTUPLE` values.
    Note only real non-negative indices work.

     *  On success returns 0, pushes saved data as a string on the top of stack.
     *  On failure returns non-zero, pushes error message on the top
        of the stack.

 * `int luabins_load(lua_State * L, const unsigned char * data,
    size_t len, int *count)`

    Load Lua values from given byte chunk.

     *  On success returns 0, pushes loaded values on stack.
        Sets count to the number of values pushed.
        Note that to have zero loaded items is a valid scenario.
     *  On failure returns non-zero, pushes error message on the top
        of the stack.

Luabins is still an experimental volatile software.
Please see source code for more documentation.

See the copyright information in the file named `COPYRIGHT`.
