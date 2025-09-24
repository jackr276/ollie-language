/**
* SSA Testing
*/


fn println(a:i32) -> void{
	a++;
	ret;
}

pub fn main() -> i32{
	let mut x:i32 = 33;
	let mut y:i32 = 3232;


	let mut abc:i32 = 3232;

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

	let mut w:i32 = x + y;

	//w = 327;
	//w = 322;

	idle;

	ret w;
}
