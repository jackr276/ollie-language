/**
* SSA Testing
*/


fn println(a:mut i32) -> void{
	a++;
	ret;
}

pub fn main(argc:i32, argv:char**) -> i32{
	let x:mut i32 = argc + 1;
	let y:mut i32 = argc + 2;


	let abc:mut i32 = argc - 1;

	defer{
	@println(abc);
	} 

	//100% useless
	if(abc == 3)  {
		abc = 2;
	} else {
		abc = 3;
		x = 3;
	}

	if(x == 32)  {
		x = x + 3222;
		y = 323;
	} else {
		x = 32;
		y = x + y + 3;
	}

	let w:mut i32 = x + y;

	//w = 327;
	//w = 322;

	idle;

	ret w;
}
