/**
* This program is made for the purposes of testing short circuiting logic
*/

fn tester(arg:i32) -> void{
	arg++;
}

//Dummy
fn test_function(arg:i32) -> i32 {
	ret arg == 4;
}

fn test_function2(arg:i32) -> i32 {
	let x:i32 = arg == 3;
	ret x;
}

pub fn main(arg:i32, argv:char**) -> i32{
	let mut x:u32 = 232;
	
	if(@test_function(arg) && @test_function2(arg)){
		x = x - 3;
	} else {
	 	x = x + 3;
		ret x;
	}

	//So it isn't optimized away
	ret x;
}
