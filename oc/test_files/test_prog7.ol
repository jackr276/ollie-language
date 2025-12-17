/**
* Author: Jack Robbins
* Saturating add test
*/

replace TEST_INT with -1;
replace my_char with 'c';

/**
 * Addition of two's complement ints that saturates to TMAX or TMIN
 * as opposed to creating a positive or negative overflow
 */
fn saturating_add(x:i32, y:i32) -> i32{
	//Find the regular sum
	let sum:mut i32 = x + y;

	//For utility, get an int where we have 1000...000
	let one_as_msb:mut i32 = 1 << (sizeof(x) - 1);

	//If x y have the same sign, XORing will cause the MSB to be 0
	//and inverting it will make it 1 if they do
	let x_y_same_sign:mut i32 = ~(x ^ y);

	//Now we can determine if x and the sum have different signs
	//using the same process, XORing will put 1 in the MSB if they do

	let x_sum_different_sign:mut i32 = x ^ sum;

	let was_overflow:i32 = (x_y_same_sign & x_sum_different_sign) >> (sizeof(x) - 1);

	let maxint_or_minint:i32 = one_as_msb ^ (sum >> (sizeof(x) -1));

	ret (~was_overflow & sum) + (was_overflow & maxint_or_minint);
}

fn tester() -> void{
	let x:mut i32 = !3;
	++x;
	ret;
}

/**
 * Demonstrate the functionality of saturating add for positive and negative overflows
 */
pub fn main() -> i32{
	let x:mut i32 = -2U;

	//Example asm inline statement
	defer {
		asm{
			push %rax;
			push %rbx;
			movq $2, %rax;
			addq $3, %rax;
			pop %rbx;
			pop %rax;
		};
	}

	defer { 
		x = x + 3;	
		@tester();
	}
	
	if(!x) {
		++x;
	} else {
		--x;
		ret x;
	}
	
	let y:i32 = 32;

	let f:f32 = 23.2;

	//Empty case statement testing
	switch(x){
		case 3 ->
			{
			x = x + 3;
			}
		default -> {
			x = x + 76;
		}
	}

	ret @saturating_add(x, y);
}
