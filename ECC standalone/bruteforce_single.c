// Qin ZHOU: Simple 
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <time.h>
#include "uthash.h"

#define MAXN 128
#define MAXL MAXN/32
#define NUM 10 //54  // number of bits of our desired prime limit
#define RANGE 256
#define MAX_U32 0xFFFFFFFF

int total = 10;	//	total sets needed

typedef struct bigNum{
	u_int32_t num[MAXL];
	int bits;
} BigNum;

typedef struct node{	// for elliptic curve
	bool inf;
	BigNum x;
	BigNum y;
} Node;

typedef struct hashtable{
	Node X;	// key
	BigNum ap;	// a * P
	BigNum bp;	// b * P
	UT_hash_handle hh;         /* makes this structure hashable */
} NodeHT;

bool debugflag = false;
bool debugflag2 = false;
NodeHT *ht = NULL;	/* important! initialize to NULL */

int rand_range(int min_n, int max_n)
{
    return rand() % (max_n - min_n + 1) + min_n;
}

bool printBigNum(BigNum n){
	int i;

	printf("%d\t", n.bits);
	for(i = 0; i < (n.bits - 1)/ 32 + 1; i++){
		printf("%08x ", n.num[i]);
	}
	printf("\n");
	return true;
}

u_int32_t checkBit(BigNum a, int bit){
	u_int32_t num = a.num[bit / 32];
	num = (num >> (bit % 32)) % 2;

	return num;
}

int compareBigNum(BigNum bn1, BigNum bn2){
	int posflag = 0;	// 0: unset or equal; k: bn1[k] > bn2[k]; -k: bn1[k] < bn2[k] highest different bit
	int i;

	if(bn1.bits > bn2.bits){
    	posflag = bn1.bits;
    }
    else if(bn1.bits < bn2.bits){
        posflag = -bn2.bits;
    }
	else{	// bn1.bits == bn2.bits
		for(i = bn1.bits - 1; i >= 0; i--){
			u_int32_t b1 = checkBit(bn1, i);
			u_int32_t b2 = checkBit(bn2, i);
			if(b1 > b2){
				posflag = i+1;
				break;
			}
			else if(b1 < b2){
				posflag = -i-1;
				break;
			}
		}
	}

	return posflag;
}

bool initBigNum(BigNum *ret){
	memset(ret, 0, sizeof(BigNum));
	ret->bits = 1;

	return true;
}

BigNum copyBigNum(BigNum n){
	BigNum ret;
	initBigNum(&ret);

	int i;
	for(i = 0; i < (n.bits - 1)/ 32 + 1; i++){
		ret.num[i] = n.num[i];
	}
	ret.bits = n.bits;

	return ret;
}

BigNum copyBigNum2(BigNum n, int s_index, int range){
	int i;
	BigNum ret;
	initBigNum(&ret);
	for(i = 0; i < range; i++){
		u_int32_t num = checkBit(n, i + s_index);
		if(i % 32 == 0){
			ret.num[i / 32] = ret.num[i / 32] ^ num;
		}
		else
			ret.num[i / 32] = ret.num[i / 32] ^ (num << (i % 32));
	}
	ret.bits = range;
	return ret;
}

BigNum rdm(int bits){
    BigNum ret;
    initBigNum(&ret);
	int i;

    // random a byte every time and combine to get bits value
	for(i = 0; i < bits; i++){
			if((i % 32 == 0) && (i / 32 == bits / 32)){ // must guarantee the highest bit is 1
				ret.num[i / 32] = 1;
			}
			else{
                ret.num[i / 32] = ret.num[i / 32] * 2 ^ (u_int32_t)rand_range(0, 1);
			}
	}

	ret.bits = bits;

    return ret;
}

BigNum rdm2(BigNum minN, BigNum maxN){ // minN <= ret < maxN
	BigNum ret;
	int i;
	while(true){	// generate that bits of random number, then check whether within this range, if not, regenerate
		initBigNum(&ret);
		int maxBits = 0;
		for(i = 0; i < maxN.bits; i++){
			int ran = rand_range(0, 1);
			if((i / 32 == (maxN.bits - 1) / 32) && ran > 0 && maxBits == 0){
				maxBits = maxN.bits - i + (i / 32) * 32;
			}
			ret.num[i / 32] = (ret.num[i / 32] << 1) ^ (u_int32_t)ran;
		}
		ret.bits = maxBits;

		if((compareBigNum(ret, minN) >= 0) && (compareBigNum(ret, maxN) < 0)){
			return ret;
		}
	}
	return ret;
}

int countIntBits(int num){
	int count = 0;
	if(num == 0){
		return 1;
	}
	while(num > 0){
		num = num / 2;
		count++;
	}
	return count;
}

BigNum sub(BigNum bn1, BigNum bn2, BigNum modN){ // substraction
	int i, j;
	int posflag = compareBigNum(bn1, bn2); 
	BigNum ret;
	initBigNum(&ret);
	
	if(posflag >= 0){ // ret is positive
		for(i = bn1.bits / 32; i >= 0; i--){
			if(bn1.num[i] >= bn2.num[i]){
				ret.num[i] = bn1.num[i] - bn2.num[i];
			}
			else{ // smaller!
				ret.num[i] = MAX_U32 - bn2.num[i] + 1 + bn1.num[i]; // +1 is awesome!
				j = i + 1;
				while(ret.num[j] == 0){
					ret.num[j] = MAX_U32;
					j++;
				}
				ret.num[j]--;
			}
		}
		// set the ret.bits
		for(i = bn1.bits - 1; i >= 0; i--){
			u_int32_t num = checkBit(ret, i);
			if(num > 0){
				ret.bits = i + 1;
				break;
			}
		}
	}
	else{	// ret is negative, add modN
		for(i = bn2.bits / 32; i >= 0; i--){
			if(bn2.num[i] >= bn1.num[i]){
				ret.num[i] = bn2.num[i] - bn1.num[i];
			}
			else{ // smaller!
				ret.num[i] = MAX_U32 - bn1.num[i] + 1 + bn2.num[i];
				j = i + 1;
				while(ret.num[j] == 0){
					ret.num[j] = MAX_U32;
					j++;
				}
				ret.num[j]--;
			}
		}
		for(i = modN.bits / 32; i >= 0; i--){
			if(modN.num[i] >= ret.num[i]){
				ret.num[i] = modN.num[i] - ret.num[i];
			}
			else{ // smaller!
				ret.num[i] = MAX_U32 - ret.num[i] + 1 + modN.num[i];
				j = i + 1;
				while(ret.num[j] == 0){
					ret.num[j] = MAX_U32;
					j++;
				}
				ret.num[j]--;
			}
		}
		// set the ret bits
		for(i = modN.bits - 1; i >= 0; i--){
			u_int32_t num = checkBit(ret, i);
			if(num > 0){
				ret.bits = i + 1;
				break;
			}
		}
	}
	return ret;
}

BigNum int2BigNum(int num, BigNum modN){ // integer to BigNum type 
	BigNum ret;
	initBigNum(&ret);	

	if(num >= 0){
		ret.num[0] = num;
		ret.bits = countIntBits(num);
	}
	else if(num < 0){
		ret.num[0] = -num;
		ret.bits = countIntBits(-num);
		ret = sub(modN, ret, modN);
	}

	return ret;
}		

BigNum int2BigNum_N(int num){	// num must >= 0
	BigNum ret;
	initBigNum(&ret);

	ret.num[0] = num;
	ret.bits = countIntBits(num);

	return ret;
}

bool even(BigNum m){
	if(m.num[0] % 2 == 0)
		return true;
	else
		return false;
}

BigNum shiftR(BigNum m){	// divide 2
	int i;
	BigNum ret = copyBigNum(m);

	for(i = 0; i < (ret.bits - 1) / 32 + 1; i++){
		ret.num[i] = ret.num[i] >> 1;
		if((i + 1) < (ret.bits - 1)/ 32 + 1){
			ret.num[i] = ret.num[i] ^ ((ret.num[i + 1] % 2) << 31);
		}
	}
	ret.bits--;

	return ret;
}

BigNum shiftL_N(BigNum a, int d){	// shift left without modN
	int i;
	BigNum ret = copyBigNum(a);
	BigNum _0;
	initBigNum(&_0);
	int gap1 = d / 32;
	if(compareBigNum(a, _0) != 0){
		if(gap1 > 0){
			for(i = (a.bits - 1) / 32; i >= 0; i--){
				ret.num[i + gap1] = ret.num[i];
			}
			for(i = 0; i < gap1; i++){
				ret.num[i] = 0;
			}
			ret.bits = ret.bits + gap1 * 32;
		}

		int gap2 = d % 32;
		for(i = (ret.bits - 1) / 32; i >= 0; i--){
			if(gap2 > 0){	// this is a bug and I don't know why ...  >> 32 will be the same
				ret.num[i + 1] = ret.num[i + 1] ^ (ret.num[i] >> (32 - gap2));
				ret.num[i] = ret.num[i] << gap2;
			}
		}

		ret.bits = ret.bits + gap2;
	}

	return ret;
}

BigNum shiftL(BigNum a, int d, BigNum modN){	// shift left with modN, with errors!
	int i;
	BigNum ret = copyBigNum(a);

	int gap1 = d / 32;
	if(gap1 > 0){
		for(i = (a.bits - 1) / 32; i >= 0; i--){
			ret.num[i + gap1] = ret.num[i];
		}
		for(i = 0; i < gap1; i++){
			ret.num[i] = 0;
		}
		ret.bits = ret.bits + gap1 * 32;
	}

	int gap2 = d % 32;
	for(i = (ret.bits - 1) / 32; i >= 0; i--){
		ret.num[i + 1] = ret.num[i + 1] ^ (ret.num[i] >> (32 - gap2));
		ret.num[i] = ret.num[i] << gap2;
	}
	ret.bits = ret.bits + gap2;

	// clear the ret.num[(modN.bits - 1) / 32]
	ret.num[(modN.bits - 1) / 32] = ret.num[(modN.bits - 1) / 32] << ((32 - modN.bits) % 32) >> ((32 - modN.bits) % 32);
	if(a.bits + d > modN.bits){
		ret.bits = modN.bits;
	}
	else
		ret.bits = a.bits + d;

	// may be ret > modN
	int posflag = compareBigNum(ret, modN);
	if(posflag >= 0){
		ret = sub(ret, modN, modN);
	}
	return ret;
}

BigNum add_N(BigNum a, BigNum b){	// add without modN
	int i;
	u_int32_t ans;
	BigNum ret;
	initBigNum(&ret);
	int max;
	if(a.bits >= b.bits){
		max = a.bits;
		ret.bits = max;
	}
	else{
		max = b.bits;
		ret.bits = max;
	}
	u_int32_t prev = 0;

	for(i = 0; i < max; i++){
		ans = prev;
		if(i < a.bits){
			ans = ans + checkBit(a, i);
		}
		if(i < b.bits){
			ans = ans + checkBit(b, i);
		}
		if(ans > 1){
			ans = ans - (u_int64_t)2;
			if((i % 32) != 0)
				ret.num[i / 32] = ret.num[i / 32] ^ (ans << (i % 32));
			else
				ret.num[i / 32] = ans;

			prev = 1;
		}
		else{
			if((i % 32) != 0)
				ret.num[i / 32] = ret.num[i / 32] ^ (ans << (i % 32));
			else
				ret.num[i / 32] = ans;

			prev = 0;
		}
	}

	if(prev == 1){
		if(max % 32 == 0)
			ret.num[max / 32] = (u_int32_t)1;
		else
			ret.num[max / 32] = ((u_int32_t)1 << (max % 32)) ^ ret.num[max / 32];
		ret.bits++;
	}

	return ret;
}


BigNum add(BigNum a, BigNum b, BigNum modN){
	BigNum ret = add_N(a, b);

	int posflag = compareBigNum(ret, modN);
	if(posflag >= 0){
		ret = sub(ret, modN, modN);
	}

	return ret;
}

bool checkBitsBigNum(BigNum n){
	int i;
	for(i = n.bits + 32; i >= n.bits - 1; i--){
		if(checkBit(n, i) > 0 && i != n.bits - 1){
			printf("checkBitsBigNum i: %d\n", i);
			return false;
		}
	}
	return true;
}

BigNum mul(BigNum a, BigNum b, BigNum modN){
	int i;
	u_int32_t num;
	BigNum temp;
	BigNum ret;
	initBigNum(&ret);

	for(i = 0; i < a.bits; i++){ // enumerate all bits of number a
		num = checkBit(a, i);
		if(num == 1){
			initBigNum(&temp);
			temp = shiftL_N(b, i);
			ret = add_N(ret, temp);
		}
	}

	if(compareBigNum(ret, modN) >= 0){
		// get the remainder of modN
		int s_index = ret.bits - modN.bits;	// index now
		BigNum dividend = copyBigNum2(ret, s_index, modN.bits);
		while(s_index >= 0){
			while(compareBigNum(dividend, modN) < 0 && s_index > 0){
				// dividend < modN, means we need 1 more bit of ret
				dividend = shiftL_N(dividend, 1);
				s_index--;
				dividend.num[0] = dividend.num[0] ^ checkBit(ret, s_index);
			}

			if(compareBigNum(dividend, modN) >= 0){
				dividend = sub(dividend, modN, modN); // use sub is fine, don't need sub_N
			}
			else
				break;	// don't need operation
		}
		ret = copyBigNum(dividend);
	}

	return ret;
}

BigNum expB(BigNum a, BigNum m, BigNum modN){ // follow the algrithm on page 14/17 of ~koc/~cs178/docx/w00a.pdf
	int i;
	BigNum ret;
	initBigNum(&ret);

	u_int32_t num = checkBit(m, m.bits - 1);

	if(num == 1){
		ret = copyBigNum(a);
	}
	else{
		ret.num[0] = 1;
		ret.bits = 1;
	}

	for(i = m.bits - 2; i >= 0; i--){
		ret = mul(ret, ret, modN);

		// get e[i]
		num = checkBit(m, i);
		if(num == 1){
			ret = mul(ret, a, modN);
		}
	}

	return ret;
}

// Miller-Rabin Primality Test
bool isPrime(BigNum n){
	int i;
	BigNum n_minus1 = copyBigNum(n);       // n - 1

	// generate n - 1
	i = 0;
	while(n_minus1.num[i] == 0){
		n_minus1.num[i] = MAX_U32;
		i++;
	}
	n_minus1.num[i]--;

	BigNum m = copyBigNum(n_minus1);
	int k = 0;
	while(even(m)){ // get odd m
		m = shiftR(m);
		k++;
	}

	BigNum _2 = int2BigNum(2, n);
	BigNum a = rdm2(_2, n_minus1);    // random a, for a^m
	BigNum b;
	b = expB(a, m, n);  // b_0 = a^m

	BigNum _1 = int2BigNum(1, n);
	BigNum _minus1 = int2BigNum(-1, n);

	if(compareBigNum(b, _1) == 0 || compareBigNum(b, _minus1) == 0){
			return true;
	}
	for(i = 1; i < k; i++){
			b = expB(b, _2, n);      // b_i = b_{i-1}^2

			if(compareBigNum(b, _1) == 0){
					return false;
			}
			else if(compareBigNum(b, _minus1) == 0){
					return true;
			}
	}

	return false;
}


BigNum genPrime(){      // generate Prime p
	BigNum prime = rdm(NUM);    // generate a random number as prime (?Improvement: odd it...)
	while(true){
		while(even(prime)){
			 prime = rdm(NUM);
		}
		if(isPrime(prime) && prime.num[0] % 4 == 3)	// need the prime == 3 (mod 4), otherwise it is hard to compute...
			return prime;
		else
			prime = rdm(NUM);
	}
	return prime;
}


bool printNode(Node P){
	if(P.inf){
		printf("Node infinity\n");
	}
	else{
		printf("x:\n");
		printBigNum(P.x);
		printf("y:\n");
		printBigNum(P.y);
	}
	return true;
}


BigNum eccGetY2(BigNum x, BigNum a, BigNum b, BigNum p){	// get y2 = x^3+ax+b (mod p)
	BigNum ret;
	initBigNum(&ret);
	BigNum _3 = int2BigNum(3, p);
	BigNum x3 = expB(x, _3, p);
	BigNum ax = mul(a, x, p);
	BigNum x3_ax = add(x3, ax, p);

	ret = add(x3_ax, b, p);

	return ret;
}


bool eccHasY(BigNum y2, BigNum p){
	BigNum e = copyBigNum(p);
	BigNum _1 = int2BigNum(1, p);
	BigNum ret;

	e = sub(e, _1, p);
	e = shiftR(e);	//e = (p - 1) / 2

	ret = expB(y2, e, p);

	if(compareBigNum(_1, ret) == 0){
		return true;
	}

	return false;
}

// we only consider p == 3 mod 4 ...
BigNum eccGetY(BigNum y2, BigNum p){
	BigNum ret;
	initBigNum(&ret);

	BigNum e = copyBigNum(p);
	BigNum _1 = int2BigNum(1, p);
	e = add_N(p, _1);
	e = shiftR(e);
	e = shiftR(e);	// e = (p+1)/4 mod p

	ret = expB(y2, e, p);

	return ret;
}


Node eccGenNode(BigNum a, BigNum b, BigNum p){
	BigNum _0 = int2BigNum(0, p);
	BigNum x = rdm2(_0, p);
	BigNum y;
	while(true){
		BigNum y2 = eccGetY2(x, a, b, p);

		if(eccHasY(y2, p)){
			y = eccGetY(y2, p);
			break;
		}
		x = rdm2(_0, p);
	}

	Node ret;
	ret.inf = false;
	ret.x = x;
	ret.y = y;

	return ret;
}

// just ignore it ??	Actually we need schoof's and enumeration to settle down the threshold of k! \
// Now it is implemented in main()
// Now we just use [0, p) as the chosen set ...
BigNum eccGetNodeOrder(Node nodep, BigNum a, BigNum b, BigNum p){
	//for(i = 0; i < )
	BigNum ret;

	return ret;
}

bool initNode(Node *ret){
	memset(ret, 0, sizeof(Node));

	return true;
}

Node copyNode(Node n){
	Node ret;
	initNode(&ret);

	ret.inf = n.inf;
	ret.x = copyBigNum(n.x);
	ret.y = copyBigNum(n.y);

	return ret;
}

bool eqlNode(Node P1, Node P2){
	if(P1.inf && P2.inf){
		return true;
	}
	else if(!P1.inf && !P2.inf){
		if(compareBigNum(P1.x, P2.x) == 0 && compareBigNum(P1.y, P2.y) == 0){
			return true;
		}
	}
	return false;
}

Node int2Node_N(int x, int y){ 	// x, y >= 0
	BigNum X = int2BigNum_N(x);
	BigNum Y = int2BigNum_N(y);
	Node ret;
	initNode(&ret);
	ret.x = copyBigNum(X);
	ret.y = copyBigNum(Y);

	return ret;
}

BigNum inv(BigNum a, BigNum p){	// using fermat's theorem to get the inverse of a!
	BigNum ret;
	initBigNum(&ret);
	BigNum _2 = int2BigNum(2, p);
	BigNum p_2 = sub(p, _2, p);
	ret = expB(a, p_2, p);	// a^(p-2) = inv(a)

	return ret;
}

Node eccAdd(Node P1, Node P2, BigNum a, BigNum b, BigNum p){
	Node ret;
	initNode(&ret);
	BigNum x1 = copyBigNum(P1.x);
	BigNum y1 = copyBigNum(P1.y);
	BigNum x2 = copyBigNum(P2.x);
	BigNum y2 = copyBigNum(P2.y);

	// for P1 = inf or P2 = inf
	if(P1.inf){
		ret = copyNode(P2);
	}
	else if(P2.inf){
		ret = copyNode(P1);
	}
	else if(eqlNode(P1, P2)){	// P1 == P2
		BigNum _3 = int2BigNum(3, p);
		BigNum _3x1_2 = mul(mul(_3, x1, p), x1, p);
		BigNum _3x2_a = add(_3x1_2, a, p);
		BigNum _2y1 = add(y1, y1, p);
		BigNum _0 = int2BigNum(0, p);
		if(compareBigNum(_0, _2y1) == 0){
			ret.inf = true;
			return ret;
		}

		BigNum m = mul(_3x2_a, inv(_2y1, p), p);
		BigNum x3 = mul(m, m, p);
		x3 = sub(x3, x1, p);
		x3 = sub(x3, x2, p);
		BigNum y3 = mul(m, sub(x1, x3, p), p);
		y3 = sub(y3, y1, p);

		ret.inf = false;
		ret.x = copyBigNum(x3);
		ret.y = copyBigNum(y3);
	}
	else{	// P1 != P2
		BigNum y2_y1 = sub(y2, y1, p);
		BigNum x2_x1 = sub(x2, x1, p);
		BigNum _0 = int2BigNum(0, p);
		if(compareBigNum(_0, x2_x1) == 0){
			ret.inf = true;
			return ret;
		}
		BigNum invx2_x1 = inv(x2_x1, p);
		BigNum m = mul(y2_y1, invx2_x1, p);
		BigNum x3 = mul(m, m, p);
		x3 = sub(x3, x1, p);
		x3 = sub(x3, x2, p);
		BigNum y3 = mul(m, sub(x1, x3, p), p);
		y3 = sub(y3, y1, p);

		ret.inf = false;
		ret.x = copyBigNum(x3);
		ret.y = copyBigNum(y3);
	}

	return ret;
}

Node eccMul(BigNum k, Node P, BigNum a, BigNum b, BigNum p){
	int i;
	Node ret;
	initNode(&ret);

	u_int32_t num = checkBit(k, k.bits - 1);

	if(num == 1){
		ret = copyNode(P);
	}	// else ret = (0, 0)

	for(i = k.bits - 2; i >= 0; i--){
		ret = eccAdd(ret, ret, a, b, p);

		// get e[i]
		num = checkBit(k, i);
		if(num == 1){
			ret = eccAdd(ret, P, a, b, p);
		}
	}

	return ret;
}

// single thread bruteforce
BigNum bruteforce(Node P, Node Q, BigNum a, BigNum b, BigNum p){
	BigNum _1 = int2BigNum(1, p);
	BigNum k = copyBigNum(_1);
	Node temp = copyNode(P);
	if(eqlNode(temp, Q)){
		return k;
	}
	while(compareBigNum(k, p) < 0){
		k = add(k, _1, p);
		temp = eccAdd(temp, P, a, b, p);
		if(eqlNode(temp, Q)){
			printf("Bruteforce Rounds:\n");
			printBigNum(k);
			return k;
		}
	}

	printf("Bruteforce Rounds:\n");
	printBigNum(k);
	return k;
}

NodeHT *findNodeHT(Node X){
	NodeHT l;
	memset(&l, 0, sizeof(NodeHT));
	l.X = copyNode(X);
	NodeHT *p;
	HASH_FIND(hh, ht, &l.X, sizeof(Node), p);
	return p;
}


bool addNodeHT(Node X_0, BigNum ap, BigNum bp){
	NodeHT *newr = (NodeHT*)malloc(sizeof(NodeHT));
	memset(newr, 0, sizeof(NodeHT));
	newr->X = copyNode(X_0);
	newr->ap = ap;
	newr->bp = bp;
	HASH_ADD(hh, ht, X, sizeof(Node), newr);

	return true;
}

bool printNodeHT(){
	NodeHT *s;
	int i = 0;
	printf("printNodeHT:\n");
	for(s = ht; s != NULL; s = s->hh.next){
		printf("i\t%d\n", i);
		printNode(s->X);
		printf("ap, bp:\n");
		printBigNum(s->ap);
		printBigNum(s->bp);
	}

	return true;
}

bool printNodeStruct(Node P){
	unsigned char *p = (unsigned char *)(&P);
	int n = sizeof(Node);
	int i;
	for(i = 0; i < n; i++){
		printf("%02x", p[i]);
	}
	printf("\n");

	return true;
}

// Actually this is not correct. Need to use schoof's to make the group prime first!
BigNum pollard(Node P, Node Q, BigNum a, BigNum b, BigNum p, BigNum P_order){	//P's order <= p
	BigNum ret;
	initBigNum(&ret);

	// generate random X_0 = ap*P + bp*Q
	BigNum _0 = int2BigNum(0, P_order);
	BigNum ap = rdm2(_0, P_order);
	BigNum bp = rdm2(_0, P_order);
	BigNum _1 = int2BigNum(1, p);
	BigNum _2 = int2BigNum(2, p);
	Node a_P = eccMul(ap, P, a, b, p);
	Node b_P = eccMul(bp, Q, a, b, p);
	Node X_0 = eccAdd(a_P, b_P, a, b, p);
	BigNum k = copyBigNum(_0);

	// insert X_0 and start the random walk
	addNodeHT(X_0, ap, bp);	// add the first Node
	unsigned int num_X;
	Node X_i = copyNode(X_0);
	num_X = HASH_COUNT(ht);
	while(true){
		k = add(k, _1, p);
		// split into 3 sets by x mod 3
		int t = rand_range(0, 2);	// this is not the same as the paper...
		if(t == 0){	// add P
			X_i = eccAdd(X_i, P, a, b, p);
			ap = add(ap, _1, P_order);
		}
		else if(t == 1){	// 2X_i
			X_i = eccMul( _2, X_i, a, b, p);
			ap = mul(ap, _2, P_order);
			bp = mul(bp, _2, P_order);
		}
		else if(t == 2){	// add Q
			X_i = eccAdd(X_i, Q, a, b, p);
			bp = add(bp, _1, P_order);
		}
		if(X_i.inf){
			printf("X_i hits infinity!\n");
			BigNum b2_b1 = sub(P_order, bp, P_order);
			BigNum inv_b2_b1 = inv(b2_b1, P_order);
			BigNum a1_a2 = copyBigNum(ap);
			ret = mul(a1_a2, inv_b2_b1, P_order);

			break;
		}

		NodeHT *r = findNodeHT(X_i);
		if(r){	// found a collision!
			if(compareBigNum(r->bp, bp) != 0){	// b1 != b2
				// d = (a1 - a2) / (b2 - b1);	a1 is r->ap
				BigNum b2_b1 = sub(bp, r->bp, P_order);
				BigNum inv_b2_b1 = inv(b2_b1, P_order);
				BigNum a1_a2 = sub(r->ap, ap, P_order);
				ret = mul(a1_a2, inv_b2_b1, P_order);
				break;
			}
		}
		else
			addNodeHT(X_i, ap, bp);

		num_X = HASH_COUNT(ht);
	}

	printf("Pollard Rounds:\n");
	printBigNum(k);

	return ret;
}

BigNum getBigNum(FILE *fp){
	BigNum ret;
	initBigNum(&ret);
	int i;
	char c = fgetc(fp);
	int bits = 0;	// how many bits ...
	int j = 1;	// j = 10 ^ ..
	while(c != '\t'){	// get bits
		bits = bits * 10 + (c - '0');
		c = fgetc(fp);
	}
	ret.bits = bits;

	int seg = (bits - 1)/ 32 + 1;
	for(i = 0; i < seg; i++){
		for(j = 0; j < 8; j++){	// 8 bytes
			c = fgetc(fp);
			int k = 0;
			if(c >= '0' && c <= '9')
				k = c - '0';
			else if(c >= 'a' && c <= 'f')
				k = c - 'a' + 10;

			ret.num[i] = ret.num[i] * 16 + k;
		}
	}

	return ret;
}

int main(int argc, char *argv[]){
	//srand(0);// for testing
	srand((unsigned)time(NULL));
	BigNum prime;
	BigNum a;
	BigNum b;
	Node P;
	BigNum k;
	Node Q;
	BigNum P_order;
	int i = 0;
	FILE *fp;
	char *line = NULL;
	size_t len = 0;

//	for(i = 0; i < total; i++){
//		while(true){
//			prime = genPrime();
//
//			// generate a stupid elliptic curve ...
//			BigNum _0 = int2BigNum(0, prime);
//			BigNum r = rdm2(_0, prime);	// generate r that 0 <= r < p
//			a = copyBigNum(r);	// stupid a = r, because we need r * b^2 = a^3 (mod p), just make a = r, b = r
//			b = copyBigNum(r);	// so now the curve is: y^2 = x^3 + ax + b (mod p) ...
//
//			// generate a stupid node
//			P = eccGenNode(a, b, prime);
//			k = rdm2(_0, prime);	// k as the private key
//			Q = eccMul(k, P, a, b, prime);	// Q = kP as the public key
//			while(Q.inf){
//				k = rdm2(_0, prime);
//				Q = eccMul(k, P, a, b, prime);
//			}
//
//			// find the P's order
//			BigNum _1 = int2BigNum(1, prime);
//			BigNum _51 = int2BigNum(51, prime);
//			P_order = copyBigNum(_1);
//			Node Pnow = copyNode(P);
//			while(!Pnow.inf){
//				if(compareBigNum(P_order, _51) == 0){
//					debugflag = false;
//				}
//				Pnow = eccAdd(Pnow, P, a, b, prime);
//				P_order = add_N(P_order, _1);
//			}
//
//			if(isPrime(P_order)){
//				break;
//			}
//		}
//
//		printf("i:\t%d\n", i);
//		printf("p:\n");
//		printBigNum(prime);
//		printf("a:\n");
//		printBigNum(a);
//		printf("b:\n");
//		printBigNum(b);
//		printf("P:\n");
//		printNode(P);
//		printf("k:\n");
//		printBigNum(k);
//		printf("Q:\n");
//		printNode(Q);
//		printf("\n");
//	}

	if(argc != 2){
		printf("Please run like this: ./cracker inputfile\n");
		exit(-1);
	}

	fp = fopen(argv[1], "r");
	// exhausting Method
	for(i = 0; i < total; i++){
		getline(&line, &len, fp);	// i:
		getline(&line, &len, fp);	// p:
		prime = getBigNum(fp);
		getline(&line, &len, fp);	// \n
		getline(&line, &len, fp);	// a:
		a = getBigNum(fp);
		getline(&line, &len, fp);	// \n
		getline(&line, &len, fp);	// b:
		b = getBigNum(fp);
		getline(&line, &len, fp);	// \n
		getline(&line, &len, fp);	// P:
		getline(&line, &len, fp);	// x:
		P.inf = false;
		P.x = getBigNum(fp);
		getline(&line, &len, fp);	// \n
		getline(&line, &len, fp);	// y:
		P.y = getBigNum(fp);
		getline(&line, &len, fp);	// \n
		getline(&line, &len, fp);	// k:
		k = getBigNum(fp);
		getline(&line, &len, fp);	// \n
		getline(&line, &len, fp);	// Q:
		getline(&line, &len, fp);	// x:
		Q.inf = false;
		Q.x = getBigNum(fp);
		getline(&line, &len, fp);	// \n
		getline(&line, &len, fp);	// y:
		Q.y = getBigNum(fp);
		getline(&line, &len, fp);	// \n
		getline(&line, &len, fp);	// P_order
		P_order = getBigNum(fp);
		getline(&line, &len, fp);	// \n
		getline(&line, &len, fp);	// \n

//		printf("i:\t%d\n", i);
//		printf("p:\n");
//		printBigNum(prime);
//		printf("a:\n");
//		printBigNum(a);
//		printf("b:\n");
//		printBigNum(b);
//		printf("P:\n");
//		printNode(P);
//		printf("k:\n");
//		printBigNum(k);
//		printf("Q:\n");
//		printNode(Q);
//		printf("P_order:\n");
//		printBigNum(P_order);
//		printf("\n");

		printf("\ni:\t%d\n", i);
		// bruteforce method
		BigNum k2 = bruteforce(P, Q, a, b, prime);
		printf("k2:\n");
		printBigNum(k2);
		printf("\n");

		// Pollard's Rho Method
		BigNum k3 = pollard(P, Q, a, b, prime, P_order);
		printf("k3:\n");
		printBigNum(k3);
	}

	return 0;
}
