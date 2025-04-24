#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>


void matmul0(double *C, double *A, double *B, int n) {
    // Matrix multiplication: C = C + A * B
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            for (int k = 0; k < n; k++) {
                C[i * n + j] += A[i * n + k] * B[k * n + j];
            }
        }
    }
}

void matmul1(double *C, double *A, double *B, int n) {
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      register double cij = C[i * n + j]; //Please do not delete "register" keyword
      for (int k = 0; k < n; k++) {
        cij += A[i * n + k] * B[k * n + j];
      }
      C[i * n + j] = cij;
    }
  }
}

void matmul2(double *C, double *A, double *B, int n) {
    // complete the missing code here
    for (int k = 0; k < n; k++) {
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                C[i * n + j] += A[i * n + k] * B[k * n + j];
            }
        }
    }
}

void matmul3(double *C, double *A, double *B, int n) {
     // complete the missing code here
     for (int j = 0; j < n; j++) {
        for (int k = 0; k < n; k++) {
            for (int i = 0; i < n; i++) {
                C[i * n + j] += A[i * n + k] * B[k * n + j];
            }
        }
    }
}

void matmul4(double *C, double *A, double *B, int n) {
     // complete the missing code here
    // for (int i = 0; i < n; i++) {
    //     for (int k = 0; k < n; k++) {
    //         register double aik = A[i * n + k];
    //         for (int j = 0; j < n; j++) {
    //             C[i * n + j] += aik * B[k * n + j];
    //         }
    //     }
    // }
    int i, j, k, ii, jj, kk;
    //int i_end, j_end, k_end;
    int BLOCK_SIZE = 32;

    // Blocked ijk loop
    for (ii = 0; ii < n; ii += BLOCK_SIZE) {
        for (jj = 0; jj < n; jj += BLOCK_SIZE) {
            for (kk = 0; kk < n; kk += BLOCK_SIZE) {                
                int j_end = (jj + BLOCK_SIZE > n) ? n : jj + BLOCK_SIZE;
                int i_end = (ii + BLOCK_SIZE > n) ? n : ii + BLOCK_SIZE;    
                int k_end = (kk + BLOCK_SIZE > n) ? n : kk + BLOCK_SIZE;
                for (i = ii; i < i_end; i++) {
                    register double acc[BLOCK_SIZE];
                    double *c_row = &C[i * n + jj];
                    for (j = 0; j < j_end - jj; j++) {
                        acc[j] = c_row[j];
                    }
                    for (k = kk; k < k_end; k++) {
                        register double aik = A[i * n + k];
                        register double *b_row = &B[k * n + jj];
                        for (j = 0; j < j_end-jj; j++) {
                            acc[j] += aik * b_row[j];
                        }
                    }
                    for (j = 0; j < j_end - jj; j++) {
                        c_row[j] = acc[j];
                    }
                }

            }
        }
    }
}

void verify_result(double *C, double *A, double *B, int n) {
    double *C_backup = new double[n * n];
    
    for (int i = 0; i < n * n; i++) {
        C_backup[i] = 1.0;
    }
    
    srand(time(NULL));
    bool all_correct = true;
    
    for (int t = 0; t < 10; t++) {
        int i = rand() % n;
        int j = rand() % n;
        
        double standard = C_backup[i * n + j];
        for (int k = 0; k < n; k++) {
            standard += A[i * n + k] * B[k * n + j];
        }
        
        double rel_error = fabs(C[i * n + j] - standard) / (fabs(standard) + 1e-20);
        
        if (rel_error > 1e-10) {
            all_correct = false;
            break;
        }
    }
    
    printf("Matrix multiplication: %s\n", all_correct ? "correct" : "incorrect");
    
    delete[] C_backup;
}

int main() {
    int n = 64;
    double* A = new double[n * n];
    double* gap1 = new double[100000];
    double* B = new double[n * n];
    double* gap2 = new double[100000];
    double* C = new double[n * n];
    for (int i = 0; i < 100000; i++) {
      gap1[i] = 1.0;
      gap2[i] = 2.0;
    }

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            A[i * n + j] = i + j;
            B[i * n + j] = i - j;
            C[i * n + j] = 1.0; 
        }
    }

    // To generate trace for matmul0:
    //matmul0(C, A, B, n);
    // For matmul1, comment out the above line and use:
    // matmul1(C, A, B, n);
    // matmul2(C, A, B, n);
    // matmul3(C, A, B, n);
     matmul4(C, A, B, n);
    
    verify_result(C, A, B, n);

    delete[] A;
    delete[] B;
    delete[] C;
    return 0;
}
