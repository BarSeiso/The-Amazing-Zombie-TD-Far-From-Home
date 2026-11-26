# Edisi Web / Browser — *The Amazing Zombie TD: Far From Home*

Folder ini berisi port **WebAssembly (WASM)** yang terpisah sepenuhnya dari kode desktop UTS (`src/main.c` tetap murni untuk Windows/Desktop).

## Cara Tayang Gratis Otomatis (GitHub Pages — Tanpa Install Apa Pun)

1. Pastikan repositori GitHub `BarSeiso/The-Amazing-Zombie-TD-Far-From-Home` berstatus **Public**.
2. Buka halaman repositori di GitHub → **Settings** → **Pages**.
3. Pada bagian **Build and deployment**, ubah **Source** menjadi **`GitHub Actions`**.
4. Lakukan `git add`, `git commit`, dan `git push` ke branch `main`:
   ```powershell
   git add .
   git commit -m "Menambahkan port WebAssembly dan deploy otomatis GitHub Pages"
   git push origin main
   ```
5. Tunggu ~1–2 menit di tab **Actions** sampai workflow **Deploy Web Game to GitHub Pages** berwarna hijau.
6. Game langsung aktif dan gratis diakses siapa saja (Laptop maupun HP) melalui:
   `https://barseiso.github.io/The-Amazing-Zombie-TD-Far-From-Home/`

---

## Cara Build Lokal di Laptop (Opsional, Jika Ingin Tes Offline)

Jika suatu saat kamu memasang [Emscripten SDK (`emsdk`)](https://emscripten.org/docs/getting_started/downloads.html):

```powershell
emcmake cmake -S web -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web
python -m http.server 8080 --directory build-web/dist
```

Lalu buka `http://localhost:8080` di browser.
