# Solves the right arm poses for the trident spin and throw (sGdfTridentArmPoses in z_boss_ganon.c) with the
# inverse kinematics helper (ik.py, from the Play as Ganondorf pose tools; it reads his skeleton from oot.o2r).
import numpy as np, json
from ik import *
T = {'over': (np.array([-700.0, 12300.0, -300.0]), None),
     'throw': (np.array([-750.0, 9500.0, 2600.0]), np.array([0, 0.3, 1.0]))}
out = {}
for name, (t, b) in T.items():
    rot, info = solve('R', t, None if b is None else b / np.linalg.norm(b), None, w_reg=0.005, avoid_body=True, w_hinge=0.02)
    out[name] = rot
    print(name, rot, 'err %.0f' % info['err_fist'], 'elbow', info['elbow'])
json.dump(out, open('tridentarm.json', 'w'))
