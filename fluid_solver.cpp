#include "fluid_solver.h"

#define IX(i, j, k) ((i) + (M + 2) * (j) + (M + 2) * (N + 2) * (k))
#define SWAP(x0, x)  \
  {                  \
    float *tmp = x0; \
    x0 = x;          \
    x = tmp;         \
  }
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define LINEARSOLVERTIMES 20

// Add sources (density or velocity)
void add_source(int M, int N, int O, float *x, float *s, float dt)
{
  int size = (M + 2) * (N + 2) * (O + 2);
  for (int i = 0; i < size; i++)
  {
    x[i] += dt * s[i];
  }
}

// Set boundary conditions
void set_bnd(int M, int N, int O, int b, float *x)
{
  int i, j;

  // Set boundary on faces
  for (i = 1; i <= M; i++)
  {
    for (j = 1; j <= N; j++)
    {
      x[IX(i, j, 0)] = b == 3 ? -x[IX(i, j, 1)] : x[IX(i, j, 1)];
      x[IX(i, j, O + 1)] = b == 3 ? -x[IX(i, j, O)] : x[IX(i, j, O)];
    }
  }
  for (i = 1; i <= N; i++)
  {
    for (j = 1; j <= O; j++)
    {
      x[IX(0, i, j)] = b == 1 ? -x[IX(1, i, j)] : x[IX(1, i, j)];
      x[IX(M + 1, i, j)] = b == 1 ? -x[IX(M, i, j)] : x[IX(M, i, j)];
    }
  }
  for (i = 1; i <= M; i++)
  {
    for (j = 1; j <= O; j++)
    {
      x[IX(i, 0, j)] = b == 2 ? -x[IX(i, 1, j)] : x[IX(i, 1, j)];
      x[IX(i, N + 1, j)] = b == 2 ? -x[IX(i, N, j)] : x[IX(i, N, j)];
    }
  }

  // Set corners
  x[IX(0, 0, 0)] = 0.33f * (x[IX(1, 0, 0)] + x[IX(0, 1, 0)] + x[IX(0, 0, 1)]);
  x[IX(M + 1, 0, 0)] =
      0.33f * (x[IX(M, 0, 0)] + x[IX(M + 1, 1, 0)] + x[IX(M + 1, 0, 1)]);
  x[IX(0, N + 1, 0)] =
      0.33f * (x[IX(1, N + 1, 0)] + x[IX(0, N, 0)] + x[IX(0, N + 1, 1)]);
  x[IX(M + 1, N + 1, 0)] = 0.33f * (x[IX(M, N + 1, 0)] + x[IX(M + 1, N, 0)] +
                                    x[IX(M + 1, N + 1, 1)]);
}

// Linear solve for implicit methods (diffusion)
void lin_solve(int M, int N, int O, int b, float *fluids, float *fluids0, float a,
               float c)
{
  const int M_b = (M + 2);
  const int MN = M_b * (N + 2);
  const float cRecip = 1.0 / c;
  const int block_size_z = 2, block_size_y = 2, block_size_x = 2;

  const int z_step = block_size_z * MN;
  const int z_limit = O * MN;
  const int z_out = (O + 1) * MN;
  const int z_step_minus_limit = z_step - z_out;

  const int y_step = block_size_y * M_b;
  const int y_limit = N * M_b;
  const int y_out = (N + 1) * M_b;
  const int y_step_minus_limit = y_step - y_out;

  const int x_out = M + 1;
  const int x_step_minus_limit = block_size_x - x_out;

  int k_end, j_end, i_end;

  int ix;
  int prior_k, next_k;
  int prior_j, next_j;
  int k_prior_j, prior_k_j, k_next_j, next_k_j;

  for (int l = 0; l < LINEARSOLVERTIMES; l++)
  {
    // Loop over blocks in the k (z-axis) direction
    for (int k_block = MN; k_block <= z_limit; k_block += z_step)
    {
      k_end = k_block + z_step - ((k_block + z_step > z_out) * (k_block + z_step_minus_limit));
      // Loop over blocks in the j (y-axis) direction
      for (int j_block = M_b; j_block <= y_limit; j_block += y_step)
      {
        j_end = j_block + y_step - ((j_block + y_step > y_out) * (j_block + y_step_minus_limit));
        // Loop over blocks in the i (x-axis) direction
        for (int i_block = 1; i_block <= M; i_block += block_size_x)
        {
          i_end = i_block + block_size_x - ((i_block + block_size_x > x_out) * (i_block + x_step_minus_limit));
          // Process elements within the current block
          for (int k = k_block; k < k_end; k += MN)
          {
            prior_k = k - MN;
            next_k = k + MN;

            for (int j = j_block; j < j_end; j += M_b)
            {
              prior_j = j - M_b;
              next_j = j + M_b;
              k_prior_j = k + prior_j;
              prior_k_j = prior_k + j;
              k_next_j = k + next_j;
              next_k_j = next_k + j;

              for (int i = i_block; i < i_end; i++)
              {
                ix = k + j + i;

                fluids[ix] = (fluids0[ix] +
                              a * (fluids[ix - 1] + fluids[ix + 1] +
                                   fluids[k_prior_j + i] + fluids[k_next_j + i] +
                                   fluids[prior_k_j + i] + fluids[next_k_j + i])) *
                             cRecip;
              }
            }
          }
        }
      }
    }
    set_bnd(M, N, O, b, fluids);
  }
}

// Diffusion step (uses implicit method)
void diffuse(int M, int N, int O, int b, float *x, float *x0, float diff,
             float dt)
{
  int max = MAX(MAX(M, N), O);
  float a = dt * diff * max * max;
  lin_solve(M, N, O, b, x, x0, a, 1 + 6 * a);
}

// Advection step (uses velocity field to move quantities)
void advect(int M, int N, int O, int b, float *d, float *d0, float *u, float *v,
            float *w, float dt)
{
  float dtX = dt * M, dtY = dt * N, dtZ = dt * O;

  for (int i = 1; i <= M; i++)
  {
    for (int j = 1; j <= N; j++)
    {
      for (int k = 1; k <= O; k++)
      {
        float x = i - dtX * u[IX(i, j, k)];
        float y = j - dtY * v[IX(i, j, k)];
        float z = k - dtZ * w[IX(i, j, k)];

        // Clamp to grid boundaries
        if (x < 0.5f)
          x = 0.5f;
        if (x > M + 0.5f)
          x = M + 0.5f;
        if (y < 0.5f)
          y = 0.5f;
        if (y > N + 0.5f)
          y = N + 0.5f;
        if (z < 0.5f)
          z = 0.5f;
        if (z > O + 0.5f)
          z = O + 0.5f;

        int i0 = (int)x, i1 = i0 + 1;
        int j0 = (int)y, j1 = j0 + 1;
        int k0 = (int)z, k1 = k0 + 1;

        float s1 = x - i0, s0 = 1 - s1;
        float t1 = y - j0, t0 = 1 - t1;
        float u1 = z - k0, u0 = 1 - u1;

        d[IX(i, j, k)] =
            s0 * (t0 * (u0 * d0[IX(i0, j0, k0)] + u1 * d0[IX(i0, j0, k1)]) +
                  t1 * (u0 * d0[IX(i0, j1, k0)] + u1 * d0[IX(i0, j1, k1)])) +
            s1 * (t0 * (u0 * d0[IX(i1, j0, k0)] + u1 * d0[IX(i1, j0, k1)]) +
                  t1 * (u0 * d0[IX(i1, j1, k0)] + u1 * d0[IX(i1, j1, k1)]));
      }
    }
  }
  set_bnd(M, N, O, b, d);
}

// Projection step to ensure incompressibility (make the velocity field
// divergence-free)
void project(int M, int N, int O, float *u, float *v, float *w, float *p,
             float *div)
{
  for (int i = 1; i <= M; i++)
  {
    for (int j = 1; j <= N; j++)
    {
      for (int k = 1; k <= O; k++)
      {
        div[IX(i, j, k)] =
            -0.5f *
            (u[IX(i + 1, j, k)] - u[IX(i - 1, j, k)] + v[IX(i, j + 1, k)] -
             v[IX(i, j - 1, k)] + w[IX(i, j, k + 1)] - w[IX(i, j, k - 1)]) /
            MAX(M, MAX(N, O));
        p[IX(i, j, k)] = 0;
      }
    }
  }

  set_bnd(M, N, O, 0, div);
  set_bnd(M, N, O, 0, p);
  lin_solve(M, N, O, 0, p, div, 1, 6);

  for (int i = 1; i <= M; i++)
  {
    for (int j = 1; j <= N; j++)
    {
      for (int k = 1; k <= O; k++)
      {
        u[IX(i, j, k)] -= 0.5f * (p[IX(i + 1, j, k)] - p[IX(i - 1, j, k)]);
        v[IX(i, j, k)] -= 0.5f * (p[IX(i, j + 1, k)] - p[IX(i, j - 1, k)]);
        w[IX(i, j, k)] -= 0.5f * (p[IX(i, j, k + 1)] - p[IX(i, j, k - 1)]);
      }
    }
  }
  set_bnd(M, N, O, 1, u);
  set_bnd(M, N, O, 2, v);
  set_bnd(M, N, O, 3, w);
}

// Step function for density
void dens_step(int M, int N, int O, float *x, float *x0, float *u, float *v,
               float *w, float diff, float dt)
{
  add_source(M, N, O, x, x0, dt);
  SWAP(x0, x);
  diffuse(M, N, O, 0, x, x0, diff, dt);
  SWAP(x0, x);
  advect(M, N, O, 0, x, x0, u, v, w, dt);
}

// Step function for velocity
void vel_step(int M, int N, int O, float *u, float *v, float *w, float *u0,
              float *v0, float *w0, float visc, float dt)
{
  add_source(M, N, O, u, u0, dt);
  add_source(M, N, O, v, v0, dt);
  add_source(M, N, O, w, w0, dt);
  SWAP(u0, u);
  diffuse(M, N, O, 1, u, u0, visc, dt);
  SWAP(v0, v);
  diffuse(M, N, O, 2, v, v0, visc, dt);
  SWAP(w0, w);
  diffuse(M, N, O, 3, w, w0, visc, dt);
  project(M, N, O, u, v, w, u0, v0);
  SWAP(u0, u);
  SWAP(v0, v);
  SWAP(w0, w);
  advect(M, N, O, 1, u, u0, u0, v0, w0, dt);
  advect(M, N, O, 2, v, v0, u0, v0, w0, dt);
  advect(M, N, O, 3, w, w0, u0, v0, w0, dt);
  project(M, N, O, u, v, w, u0, v0);
}
