/**
* Bad parens in here
*/

define struct my_struct{
	i:u32*;
	a:i8;
	c:i8;
} as aliased_struct;

pub fn main(argc:i32, argv:char**) -> i32 {
	let i:u32 = 0;
	declare j:u32;

	j = i;

	let k:u32 = 0;
	let l:u32 = 0;
	declare float_arr:f32[10];

	if(i == 0) {
		i = 2;
	} else {
		if(i == 3) {
			i = 3;
		}
	}
}
