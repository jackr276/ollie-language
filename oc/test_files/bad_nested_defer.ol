/**
* Author: Jack Robbins
* Testing scenarios with illegal nested defers
*/

pub fn main() -> i32 {
	let x:mut i32 = 0;
	let y:mut i32 = 0;
	let z:mut i32 = 0;

	while ( x <= 3) {
		x++;
		defer {
			y--;
		};
	}


	//Do while loop
	do {
		x += y;
		defer {
			x -= 43;
		};

	} while ( x < 3);

	//For loop
	for(let i:mut i32 = 0; i < 89; i++) {
		x++;

		defer {
			z += 6;
		};
	}

	//Double nest
	if (x == 0)  {
		if ( y == 0)  {
			defer {
				x--;
			};
		}
	}

	
	ret x;
}
