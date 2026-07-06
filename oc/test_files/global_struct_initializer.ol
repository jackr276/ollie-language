/**
* Author: Jack Robbins
* Test our ability to initialize a global struct using a struct intializer
*/

define struct my_struct {
	c:mut char;
	d:mut i16;
	arr:mut i32[5];
	f:mut f64;
};


//Trigger the global struct initializer here
let global_struct:struct my_struct = {'a', 2, [1, 2, 3, 4, 5], 4.44d};


pub fn main() -> i32 {
	//Should return 'a'(97) + 3 + 5 = 105
	OUNIT: [exit_status = 105]
	ret global_struct:c + global_struct:arr[2] + global_struct:arr[4];
}
