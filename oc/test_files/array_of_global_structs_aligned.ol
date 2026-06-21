/**
* Author: Jack Robbins
* Test an array of global structs and how we handle it. The structs are designed to need
* a lot of padding for alignment
*/


/**
* Struct should be: 1 + 7 pad + 8 + 1 + 7 pad + 8 = 32 bytes large
*/
define struct my_struct {
	a:mut char;
	b:mut i64;
	c:mut i8;
	d:mut i32;
};


let array_of_global_structs:struct my_struct[] = [{'a', 12, 9, 5}, {'b', 13, 12, 11}, {'c', 14, 20, 9}];


pub fn main() -> i32 {
	//Should return 97('a') + 11 + 9 = 117
	OUNIT: [console = 117]
	ret array_of_global_structs[0]:a + array_of_global_structs[1]:d + array_of_global_structs[2]:d;
	
}
