/**
 * Author: Jack Robbins
 * Multifile dependency checker program that is designed to
 * test our ability to pull dependencies multiple levels deep
 */

$import "adder";
$import "subber";


pub fn main() -> i32 {
	let x:i32 = 5;
	let y:i32 = 6;

	//Should be 11 + 1
	OUNIT: [exit_status = 12]
	ret @adder(6, 5) + @subber(6, 5);
}
