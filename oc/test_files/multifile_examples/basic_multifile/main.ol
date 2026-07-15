/**
 * Author: Jack Robbins
 * A basic multifile compilation tester that tests our ability to link stuff together
 */

//The using macro declares that we want to pull in the test_dependency module
$import "test_dependency";

 pub fn main() -> i32 {
 	OUNIT: [exit_status = 9]
 	ret @add(5, 4);
 }
