/**
* Bad parens in here
*/
define construct my_struct{
	u32* i;
	char a;
	i8 c;
} as aliased_struct;

fn:static main(i32 argc, char** argv) -> i32 {
	let u32 i := 0;
	declare u32 j;

	j := i;

	let u32 k := 0;
	let u32 l := 0;
	declare f32[10] float_arr;

	if(i == 0) then {
		i := 2;
	} else {
		if(i == 3) then {
			i := 3;
		}
	}
}
