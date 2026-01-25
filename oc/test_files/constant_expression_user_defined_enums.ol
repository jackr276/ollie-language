/**
* Author: Jack Robbins
* Test user defined enums parsing constant-expressions
*/

define enum type_size {
	TYPE_SIZE_CHAR = typesize(char),
	TYPE_SIZE_SHORT = typesize(i16),
	TYPE_SIZE_INT = typesize(i32),
	TYPE_SIZE_LONG = typesize(i64)
};

pub fn get_sizes(x:i32) -> enum type_size {
	switch(x){
		case 1 -> {
			ret TYPE_SIZE_CHAR;
		}
		
		case 2 -> {
			ret TYPE_SIZE_SHORT;
		}

		case 4 -> {
			ret TYPE_SIZE_INT;
		}

		case 8 -> {
			ret TYPE_SIZE_LONG; 
		}

		default -> {
			ret TYPE_SIZE_LONG;
		}
	}
	
}

pub fn main() -> i32 {
	ret 0;
}
