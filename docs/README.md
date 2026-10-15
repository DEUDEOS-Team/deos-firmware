# DEOS Documentation & Architecture Center

Dokümantasyon, Mimari Tasarım ve Arayüz Kontrol Dokümanları (ICD - Interface Control Documents) merkezi deposudur.

Bu depo, **DEUDEOS Otonom Araç Ekibi** bünyesindeki tüm alt sistemlerin (Firmware, ROS 2, Yer İstasyonu, Simülasyon vb.) ortak protokol ve mimari referansını barındırır.

---

## 📁 Dizin Yapısı

- **[`icd/`](icd/)**: CAN-FD ve araç içi haberleşme protokolünü tanımlayan resmi LaTeX ICD kaynakları ve derleme betikleri (`build_icd.sh`).
- **[`DEOS_ARCHITECTURE_KNOWLEDGE.md`](DEOS_ARCHITECTURE_KNOWLEDGE.md)**: Zero-latency optimizasyonları, Split Horizon yönlendirme kuralları ve ağ topolojisi kritik kuralları.
- **[`QUICK_REFERENCE.md`](QUICK_REFERENCE.md)**: Node ID'leri, Servis ID'leri ve Little-Endian bayt dizilimini özetleyen hızlı mühendislik tablosu.
- **[`FUNCTIONAL_SPLIT.md`](FUNCTIONAL_SPLIT.md)**: Alt sistemlerin sorumluluk sınırları ve mantıksal iş bölümü.
- **[`ros2/`](ros2/)**: ROS 2 köprüsü ve telemetri entegrasyonu rehberleri.
- **[`reports/`](reports/)**: Test, analiz ve doğrulama raporları.
- **[`archive/`](archive/)**: Protokolün eski sürümleri ve arşiv dokümanları.

---

## 🔄 Firmware Projesi ile Senkronizasyon (Git Subtree)

Bu dokümantasyon deposu hem bağımsız olarak (`DEUDEOS-Team/deos-docs`) klonlanabilir hem de `deos-firmware` projesinde `docs/` klasörü altında **Git Subtree** ile birebir senkronize çalışır.

### Firmware Reposundan Dokümanları Güncellemek (Push)
Firmware üzerinde dokümantasyonda yapılan değişiklikleri bu depoya göndermek için:
```bash
git subtree push --prefix=docs deos-docs main
```

### Bu Depodaki Güncellemeleri Firmware Reposuna Çekmek (Pull)
Harici olarak bu depoya eklenen değişiklikleri firmware reposuna dahil etmek için:
```bash
git subtree pull --prefix=docs deos-docs main --squash
```

---

## 🚀 Sadece Dokümantasyonu Kullanmak (Firmware Olmadan)

Eğer firmware kodlarına ihtiyacınız yoksa (örneğin ROS 2 ekibi, Arayüz/Yer İstasyonu geliştiricileri veya doküman yazarları), tüm firmware projesini ve Zephyr ortamını indirmeden **yalnızca bu depoyu** bağımsız olarak kullanabilirsiniz:

### 1. Depoyu Bağımsız Klonlama
```bash
git clone https://github.com/DEUDEOS-Team/deos-docs.git
cd deos-docs
```

### 2. Değişiklik Yapma ve Gönderme
Doğrudan bağımsız bir Git deposu gibi çalışarak commit ve push atabilirsiniz:
```bash
git add .
git commit -m "docs: dokümantasyon güncellendi"
git push origin main
```
*(Firmware geliştiricileri yukarıdaki `git subtree pull` komutunu çalıştırdığında buraya yaptığınız commit'ler firmware reposuna otomatik yansıyacaktır.)*

### 3. Diğer Alt Sistem Projelerinde Alt Modül (Submodule) Olarak Kullanma
Örneğin `deos-ros2-bridge` veya Yer İstasyonu projenizin içine bu ortak ICD ve dokümanları bağlamak isterseniz:
```bash
git submodule add https://github.com/DEUDEOS-Team/deos-docs.git docs
```

