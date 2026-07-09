/**
 * Author: Jack Robbins
 * Test an Ollie style in statement that is exhaustive, meaning that it encompasses
 * every single value within the defined type range
 */

/**
* An enum class
*/
define enum type_enum{
	TYPE_ONE,
	TYPE_TWO,
	TYPE_THREE,
	TYPE_FOUR,
	TYPE_FIVE
} as my_enum_type;


pub fn main() -> i32 {
	let x:i32 = 3;

	OUNIT: [exit_status = 1]
	ret x in (TYPE_ONE, TYPE_TWO, TYPE_THREE, TYPE_FOUR, TYPE_FIVE);
}

