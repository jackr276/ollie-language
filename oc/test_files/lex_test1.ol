/**
* Sample program
*/

declare u_int8 j;

alias u_int8[100] as int_arr;

func:static main() -> u_int32* {
	//Single line comment
	declare int_arr j_arr;
	asn j_arr[0][2] := 2;


    let u_int8* i_ptr := &i; //Another comment
	
	let u_int32 main := 1; //Illegal type redef

	let u_int8 j /*also single line comment */:= 1;
	let u_int8* j_ptr := &j;

	ret *i + *j;
}
