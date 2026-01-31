/**
* Author: Jack Robbins
* Test warning handling when we are calling functions indirectly. Even though these functions
* may not be called directly, printing warnings still is not a good option
*/

fn add(x:i32, y:i32) -> i32 {
	ret x + y;
}

fn subtract(x:i32, y:i32) -> i32 {
	ret x + y;
}

fn multiply(x:i32, y:i32) -> i32 {
	ret x * y;
}

fn divide(x:i32, y:i32) -> i32 {
	ret x / y;
}


fn dynamic_dispatch(x:u8, y:i32) -> i32{
	//Let this be a dynamic function
	define fn(i32, i32) -> i32 as dynamic_function;

	declare dyn_fun:dynamic_function;

	//TODO all wrong
	if(x == 0) {
		dyn_fun = add;

	} else if( x == 1) {
		dyn_fun = add;
	}


	switch(x) {
		case 0 -> {
			dyn_fun = add;
		}

		case 1 -> {
			dyn_fun = subtract;
		}

		case 2 -> {
			dyn_fun = multiply;
		}

		case 4 -> {
			dyn_fun = divide;
		}

		default -> {
			ret -1;
		}
	}

	ret @dyn_fun(3, y);
}



pub fn main() -> i32 {
	ret @dynamic_dispatch(1, 55);
}
