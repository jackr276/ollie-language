/**
 * Author: Jack Robbins
 * Test a case where we have a return by copy function that returns a return by copy function
 * and verify that we can do this correctly
 */

define struct my_struct {
	x:mut i32;
	y:i64;
	z:f64;
	c:mut i32[5];
} ;

define error dummy_error_t;


fn level1(x:i32, y:i32) -> struct my_struct {
	ret {x, y, 5.555, [x, y, x, y, x]};
}


//Dummy file that just throws errors
fn! throw_error(error_cond:i32) -> i32 raises (dummy_error_t){
	if(error_cond == 5){
		raise dummy_error_t;
	}

	if(error_cond == 4){
		raise error;
	}

	ret error_cond;
}


fn level2(x:i32, y:i32) -> struct my_struct {
	//RRF eligibility here
	let result:i32 = @throw_error(5) 
					 handle (dummy_error_t => ret @level1(x, y),
					 		error => ret @level1(y, x));


	ret {0, 0, 0, [0, 0, 0, 0, 0]};
}


pub fn main() -> i32 {
	let x:i32 = 5;
	let y:i32 = 6;

	//These are always going to hit the error case
	let result1:struct my_struct = @level1(x, y);
	let result2:struct my_struct = @level2(x + 5, y - 3);

	//Should return 5 + 6 + 5 + 10 + 3 + 10 = 39
	OUNIT: [exit_status = 39]
	ret result1:x + result1:c[1] + result1:c[2]
		+ result2:x + result2:c[1] + result2:c[2];
}
