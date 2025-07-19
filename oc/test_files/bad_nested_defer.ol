/**
* Author: Jack Robbins
* Testing scenarios with illegal nested defers
*/

fn main() -> i32 {
	let mut x:i32 := 0;
	let mut y:i32 := 0;
	let mut z:i32 := 0;

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
	for(let mut i:i32 := 0; i < 89; i++) {
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
