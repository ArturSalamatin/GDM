import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import json, os, sys

d = sys.argv[1]
meta = json.load(open(os.path.join(d, 'metadata.json')))
nx, ny, nz = meta['Nx'], meta['Ny'], meta.get('Nz', 1)
lx, ly = meta['Lx'], meta['Ly']
times = meta['save_times']
step = len(times) - 1

fig = plt.figure(figsize=(16, 7))
gs = fig.add_gridspec(2, nz + 1, width_ratios=[1]*nz + [0.05],
                      wspace=0.35, hspace=0.30,
                      left=0.06, right=0.95, top=0.92, bottom=0.06)

im_sw = im_p = None
for k in range(nz):
    fname = os.path.join(d, f'snapshot_{step:03d}_layer_{k}.csv')
    data = np.genfromtxt(fname, delimiter=',', skip_header=1)
    sw = data[:, 2].reshape(ny, nx)
    p = data[:, 3].reshape(ny, nx)

    ax1 = fig.add_subplot(gs[0, k])
    im_sw = ax1.imshow(sw, origin='lower', cmap='Blues', vmin=0, vmax=1,
                       extent=[0, lx, 0, ly], aspect='equal')
    ax1.set_title(f'Sw layer {k}', fontsize=9)

    ax2 = fig.add_subplot(gs[1, k])
    im_p = ax2.imshow(p, origin='lower', cmap='RdYlBu_r',
                      extent=[0, lx, 0, ly], aspect='equal')
    ax2.set_title(f'P layer {k}', fontsize=9)

    if k == 0:
        ax1.set_ylabel('y, m')
        ax2.set_ylabel('y, m')
    ax2.set_xlabel('x, m')

cax1 = fig.add_subplot(gs[0, nz])
cax2 = fig.add_subplot(gs[1, nz])
fig.colorbar(im_sw, cax=cax1, label='Sw')
fig.colorbar(im_p, cax=cax2, label='P, atm')
fig.suptitle('t = %.0f days (final)' % times[step], fontsize=13)

out = os.path.join(d, 'final_frame.png')
fig.savefig(out, dpi=100, bbox_inches='tight')
print('Saved:', out)
