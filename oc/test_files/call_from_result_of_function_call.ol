/**
 * Author: Jack Robbins
 * Test our treatment of functions as first-class citizens by calling a function that's the return
 * value of another function
 */


/**
* Shares the same signature as subtract
*/
fn add(x:i32, y:i32) -> i32{
	ret x + y;
}


/**
* Shares the same signature as add
*/
fn subtract(x:i32, y:i32) -> i32{
	ret x - y;
}


/**
* Shares the same signature as add
*/
fn multiply(x:i32, y:i32) -> i32{
	ret x * y;
}


/**
* Shares the same signature as add
*/
fn divide(x:i32, y:i32) -> i32{
	ret (y != 0) ? (x / y) else 0;
}


/**
 * Helper that will return a reference to a function
 */
pub fn function_picker(vote:i32) -> fn(i32, i32) -> i32 {
	switch(vote) {
		case 1 -> {
			ret add;
		}

		case 2 -> {
			ret subtract;
		}

		case 3 -> {
			ret multiply;
		}

		case 4 -> {
			ret divide;
		}

		default -> {
			ret add;
		}
	}
}


pub fn main() -> i32 {
	let x:i32 = 5;
	let y:i32 = 8;

	//Should return 5 + 8 + 5 * 8 = 53
	OUNIT: [exit_status = 53]
	ret @(@function_picker(1))(x, y) + @(@function_picker(3))(x, y);
}
