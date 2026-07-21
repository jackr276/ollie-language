/**
 * Author: Jack Robbins
 * A basic multifile compilation tester that tests our ability to link stuff together
 */

//The using macro declares that we want to pull in the test_dependency module
$import "add_dependency";
$import "sub_dependency";
$import "mul_dependency";
$import "div_dependency";

 pub fn main() -> i32 {
 	//Should return: 9 + 1 + 30 + 2 = 42
 	OUNIT: [exit_status = 42]
 	ret @add(5, 4) + @sub(2, 1) + @mul(5, 6) + @div(10, 5);
 }
