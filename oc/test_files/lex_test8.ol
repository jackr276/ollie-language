/**
* Bad parens in here
*/
define construct my_struct{
	u_int32* i;
	char a;
	s_int8 c;
} as aliased_struct;

func:static main(s_int32 argc, char** argv) -> u_int32 {
	let u_int32 i := 0;
	let u_int32 j := 0;
	let u_int32 k := 0;
	let u_int32 l := 0;
	declare float32[10] float_arr;

	if(i == 0) then {
		asn i := 2;
	} else {
		if(i == 3) then {
			asn i := 3;
		}
	}
}
