/**
* Author: Jack Robbins
* Create a 2D array traversal using ollie loops to prove out the concept
*/



pub fn ollie_loops(arr:i32[3][5]) -> i32 {
	let sum:mut i32 = 0;
	let row:mut i32 = 0;

	loop {
		let col:mut i32 = 0;
		
		//Inner column traversal
		loop {
			sum += arr[row][col];
			col++;
			break when(col == 3);
		}
		
		row++;
		break when(row == 3);
	}

	ret sum;
}



pub fn main() -> i32 {
	let arr:i32[3][5] = [[1, 2, 3, 4, 5], [6, 7, 8, 9, 10], [11, 12, 13, 14, 15]];
	//Sum should be 15 + 40 + 65 = 120
	OUNIT: [console = 120]
	ret @ollie_loops(arr);
}
