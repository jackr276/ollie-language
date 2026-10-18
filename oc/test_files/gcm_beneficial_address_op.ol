/**
 * Author: Jack Robbins
 * Write out an addressing operation where global code motion(GCM) would be beneficial for us
 */


pub fn main() -> i32 {
	let x:mut i32[] = [1, 2, 3, 4, 5];

	for(let i:mut i32 = 0; i < 5; i++){
		x[i] = (i % 2 == 0) ? i * 2 else i + 2;
	}

	OUNIT: [exit_status = 5]
	ret x[2];
}
