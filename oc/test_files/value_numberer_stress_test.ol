/**
* Author: Jack Robbins
* Test if the value numberer is able to identify simplification across
* multiple binary operations. It is *ok* if it is not at this point, we
* just want to see
*/

pub fn value_number_simplify(x:i32, y:i32, z:i32) -> i32{
	//Ideally we should recognize that 1 and 2 are identical
	let result1:i32 = x + y * z;
	let result2:i32 = x + y * z;

	ret result1 + result2;
}


pub fn main() -> i32 {

}
