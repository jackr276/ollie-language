**
* testing pointer arithmetic
*/

func:nested_conditionals(u_int32 input) -> str {
    if(input > 10) then {
        if(input < 20) then {
            ret "Between 10 and 20";
        } else {
            ret "Greater than or equal to 20";
        }
    } else {
        ret "10 or less";
    }
}