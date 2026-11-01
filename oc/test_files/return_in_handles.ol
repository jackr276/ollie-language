/**
 * Author: Jack Robbins
 */


define error dummy_error_t;


fn level1(x:i32, y:i32) -> i32 {
	ret x - y;
}


//Dummy function that just throws errors
fn! throw_error(val:i32, error_cond:i32) -> i32 raises (dummy_error_t){
	if(error_cond == 5){
		raise dummy_error_t;
	}

	if(error_cond == 4){
		raise error;
	}

	ret val + error_cond; 
}


pub fn main() -> i32 {
	let x:i32 = 5;
	let y:i32 = 6;

	let result1:i32 = @throw_error(x, 5) 
						handle (dummy_error_t => @level1(x, y),
					 			error => @level1(y, x));

	let result2:i32 = @throw_error(y, 4) 
						handle (dummy_error_t => @level1(x + 5, y - 4),
					 			error => @level1(y + 5, x - 4));

	
	//SHould return -1 + 10 = 9
	OUNIT: [exit_status = 9]
	ret result1 + result2;
}
