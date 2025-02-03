/**
* Bad parens in here
*/
define construct my_struct{
	u32* i;
	char a;
	i8 c;
} as aliased_struct;

func:static main(i32 argc, char** argv) -> i32 {
	let constant u32 i := 0;
	declare constant u32 j;

	asn j := i;

	let u32 k := 0;
	let u32 l := 0;
	declare float32[10] float_arr;

	if(i == 0) then {
		asn i := 2;
	} else {
		if(i == 3) then {
			asn i := 3;
		}
	}
}
