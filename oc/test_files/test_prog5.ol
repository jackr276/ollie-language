/**
 * Additiong of two's complement ints that saturates to TMAX or TMIN
 * as opposed to creating a positive or negative overflow
fn saturating_add(x:i32, y:i32) -> i32{
	//Find the regular sum
	let mut sum:i32 := x + y;

	//To find the size of the int in bytes(I don't want to assume it's 32 always), 
	//multiply by 8 by shifting right by 3(*2 *2 *2)
	let mut num_bits:i32 := 4 << 3;
	
	//For utility, get an int where we have 1000...000
	let mut one_as_msb:i32 := 1 << (num_bits - 1);

	//If x y have the same sign, XORing will cause the MSB to be 0
	//and inverting it will make it 1 if they do
	let mut x_y_same_sign:i32 := ~(x ^ y);

	//Now we can determine if x and the sum have different signs
	//using the same process, XORing will put 1 in the MSB if they do
	let mut x_sum_different_sign:i32 := x ^ sum;

	let mut was_overflow:i32 := (x_y_same_sign & x_sum_different_sign) >> (num_bits - 1);


	let mut maxint_or_minint:i32 := one_as_msb ^ (sum >> (num_bits -1));

	ret (~was_overflow & sum) + (was_overflow & maxint_or_minint);
}
*/

fn tester() -> void{
	let mut x:i32 := 3;
	++x;
	ret;
}

/**
 * Demonstrate the functionality of saturating add for positive and negative overflows
 */
fn main() -> i32{

	let mut x:i32 := 2;

	defer x + 3;	
	
	if(!x) then {

		$label1:
		++x;
	} else {
		--x;
		jump $label1;
		ret x;
	}
	
	let y:i32 := 32;

	let f:f32 := 23.2;

	switch on(x){
		case 1:
		case 2:
		case 3:
			{
			x := x + 3;
			break;
			}
		default:
			x := x + 3;
		default:
			x := x + 3;

	}

	ret 0;
}
