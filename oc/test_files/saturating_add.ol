/**
* Author: Jack Robbins
* Demonstrate the functionality of a saturating 
*/

/**
 * Addition of two's complement ints that saturates to TMAX or TMIN
 * as opposed to creating a positive or negative overflow
 */
fn saturating_add(x:i32, y:i32) -> i32{
	//Find the regular sum
	let sum:i32 = x + y;

	//For utility, get an int where we have 1000...000
	let one_as_msb:i32 = 1 << (<i32>sizeof(x) - 1);

	//If x y have the same sign, XORing will cause the MSB to be 0
	//and inverting it will make it 1 if they do
	let x_y_same_sign:i32 = ~(x ^ y);

	//Now we can determine if x and the sum have different signs
	//using the same process, XORing will put 1 in the MSB if they do

	let x_sum_different_sign:i32 = x ^ sum;

	let was_overflow:i32 = (x_y_same_sign & x_sum_different_sign) >> (<i32>sizeof(x) - 1);

	let maxint_or_minint:i32 = one_as_msb ^ (sum >> (<i32>sizeof(x) -1));

	ret (~was_overflow & sum) + (was_overflow & maxint_or_minint);
}


/**
 * 
 */
pub fn main() -> i32{
	let x:i32 = 1 << 29;
	let y:i32 = (1 << 30) + 1;
	
	/**
	 * The addition should have saturated and we should
	 * have not had a signed overflow
	 */
	OUNIT: [exit_status = 0]
	ret @saturating_add(x, y) & (1 << 31);
}
