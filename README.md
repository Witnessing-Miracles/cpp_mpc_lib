# MPC CMP: Threshold ECDSA Explained

> Based on: [*UC Non-Interactive, Proactive, Threshold ECDSA*](https://eprint.iacr.org/2020/492.pdf) (Canetti, Makriyannis, Peled — 2020/492)  
> Implementation: [Fireblocks mpc-lib](https://github.com/fireblocks/mpc-lib)

---

## What Problem Does This Solve?

In cryptocurrency custody, someone has to hold the private key. But a single private key is a single point of failure — if the server gets hacked, the key is gone.

The naive solution of splitting a key into parts doesn't work either: you still need to reassemble the full key somewhere to sign, and that reassembly moment is the attack surface.

**Threshold ECDSA** solves this by allowing `n` parties to collaboratively produce a valid ECDSA signature without the full private key ever existing in one place — not during setup, not during signing, never.

---

## Why Is Threshold ECDSA Hard?

Standard ECDSA signing computes:

```
σ = k⁻¹ · (m + r·x)  mod q
```

Where `x` is the private key and `k` is a fresh random nonce per signature.

The difficulty is the **multiplication** of secret values held by different parties. Addition is easy in distributed settings (parties can just add their shares), but multiplication requires interaction and leaks information if done naively.

Specifically:
- `k` must be secret (reusing `k` reveals the private key)
- `x` must be secret (it's the private key)
- Computing `k⁻¹ · (m + r·x)` requires multiplying shares of `k` and `x` together

All prior protocols needed **8–9 rounds** of interaction just to handle this multiplication safely. CMP brings it down to **4 rounds online**, or **3 rounds preprocessing + 1 round signing**.

---

## The Four Phases of CMP

### Phase 1 — Key Generation (run once)

Each party `Pᵢ` independently picks a random secret share `xᵢ`. The actual private key is their sum:

```
x = x₁ + x₂ + ... + xₙ
```

Nobody ever computes `x` directly. The public key `X = g^x` is derived collaboratively without revealing any `xᵢ`. Each party proves knowledge of their own share using a Schnorr zero-knowledge proof.

---

### Phase 2 — Key Refresh (run periodically)

This is one of CMP's key contributions over prior work.

Periodically, parties re-randomize their secret shares:

```
Before: x₁=5,  x₂=7,  x₃=3   (sum = 15)
After:  x₁'=2, x₂'=9, x₃'=4  (sum still = 15)
```

The public key stays the same. Old shares become worthless.

**Why this matters:** An attacker who slowly compromises one server per month can never accumulate enough shares if the shares rotate faster than the attack. This property is called **Proactive Security** — none of the prior threshold ECDSA protocols supported it.

This phase also generates auxiliary cryptographic parameters (Paillier keys, Ring-Pedersen parameters) used in signing.

---

### Phase 3 — Pre-Signing (before message is known)

This is the computationally heavy phase, and it can be done in advance before any message arrives.

The goal: compute a shared nonce point `R = g^(k⁻¹)` where `k = k₁ + k₂ + ... + kₙ`, without any party learning `k`.

**The core trick — Paillier homomorphic encryption:**

To multiply two secret values held by different parties:

1. `Pᵢ` encrypts their value `γᵢ` under their Paillier public key and sends it to `Pⱼ`
2. `Pⱼ` uses the *homomorphic property* of Paillier to compute `enc(γᵢ · kⱼ - β)` — a masked product — without ever seeing `γᵢ`
3. `Pᵢ` decrypts and gets `γᵢ · kⱼ - β`; `Pⱼ` holds `β`
4. Together they hold an additive sharing of `γᵢ · kⱼ`

This multiplication-via-encryption technique is repeated across all pairs of parties to build up shares of `k·γ`, from which `R` can be derived.

Each step is accompanied by **Non-Interactive Zero-Knowledge (NIZK) proofs** — each party proves their encrypted values are correctly formed and within valid ranges, without revealing the values themselves. This prevents a malicious party from injecting bad values that could leak other parties' secrets.

At the end of pre-signing, each party stores a tuple `(R, kᵢ, χᵢ)` — their share of the nonce and their share of `k·x`.

---

### Phase 4 — Signing (after message is known)

Once the message `m` is available, signing becomes trivial and **non-interactive**:

```
Each party Pᵢ computes and broadcasts:  σᵢ = kᵢ·m + r·χᵢ
Final signature:  σ = σ₁ + σ₂ + ... + σₙ
```

Each party sends exactly one message. No further interaction needed. This is the **non-interactive signing** that gives the paper its name, and it's what makes CMP compatible with cold wallet architectures (offline devices only need to participate in pre-signing, not real-time signing).

---

## Comparison With Prior Work

| Protocol | Signing Rounds | Proactive Refresh |
|---|---|---|
| Gennaro & Goldfeder (2018) | 9 | ❌ |
| Lindell et al. (2018) | 8 | ❌ |
| Doerner et al. (2019) | log(n)+6 | ❌ |
| **CMP — Online** | **4** | **✅** |
| **CMP — Non-Interactive** | **3 (pre) + 1 (sign)** | **✅** |

---

## Security Guarantees

The protocol is proven secure under the **UC (Universally Composable) framework**, meaning it remains secure even when composed with arbitrary other protocols — important for real-world systems where many cryptographic components interact.

Security relies on three assumptions:
- **Semantic security of Paillier encryption** — ciphertexts reveal nothing about plaintexts
- **Strong RSA assumption** — computing roots modulo an unknown-factorization RSA modulus is hard
- **Existential unforgeability of ECDSA** — the underlying signature scheme is sound

The adaptive corruption model is also supported: an attacker can compromise parties at any time during the protocol's lifetime, not just at the start.

---

## Why This Matters for Custody

CMP was designed with cryptocurrency custody in mind. The combination of:

- **No single point of failure** — private key never assembled in one place
- **Proactive key refresh** — limits the window for adaptive attacks
- **Non-interactive signing** — cold wallet devices can participate offline
- **UC security** — composable with other system components safely

...makes it the protocol of choice for institutional-grade MPC wallets. Fireblocks' open-source `mpc-lib` is their production implementation of this exact paper.