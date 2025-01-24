/**
* Sample program
*/

declare u_int8 j;

alias u_int8[100] as int_arr;

func:static main() -> u_int32* {
	//Single line comment
	declare int_arr j_arr;
	let s_int8 my_int := 1;

	declare s_int32[32] my_array;
	
	let u_int8 i := my_array || my_int;

    let u_int8* i_ptr := &i; //Another comment
	
	let u_int32 main := 1; //Illegal type redef

	let u_int8 j /*also single line comment */:= 1;
	let u_int8* j_ptr := &j;

	ret *i + *j;
}
