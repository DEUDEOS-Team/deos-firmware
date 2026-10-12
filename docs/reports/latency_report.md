# DEOS Firmware Latency ve Performans Raporu

## 1. Genel Bakış
Bu belge, DEOS (Distributed Embedded Operating System) haberleşme mimarisinin PING/PONG testleri üzerinden elde edilen donanım ve yazılım gecikme (latency) sürelerini analiz etmektedir. 

## 2. Test Senaryosu ve Ortam
- **Test Aracı:** `tools/lora_test_suite/ping_sender.py` (Yer Kontrol - 0x02)
- **Hedef Donanım:** STM32 Main Node (0x03)
- **Paket Boyutu:** 18 Byte (2 Byte Magic + 1 Byte Flag + 1 Byte Len + 8 Byte DEOS Header + 4 Byte Payload + 2 Byte CRC)
- **İşleyiş:** Python aracı PING atar, STM32'deki donanım kesmesi paketi yakalar, DEOS Router paketi çözer, PONG komutunu üretir ve tekrar hatta basar.

## 3. Ham UART Performansı (Direkt Kablo Testi)
LoRa modülü aradan çıkarılarak, bilgisayar ve STM32 doğrudan UART-USB dönüştürücü ile bağlandığında elde edilen sonuçlar:

- **Baud Rate:** 115200 bps
- **Ölçülen Ortalama Gidiş-Dönüş Süresi (RTT):** `6 ms` (Tam olarak stabil)

### Analiz ve Kırılım:
- 115200 baud hızında 18 Byte verinin fiziksel olarak bakır kablodan akması yaklaşık **1.5 ms** sürer.
- Gidiş (1.5 ms) + Dönüş (1.5 ms) = **3 ms** saf donanımsal aktarım süresidir.
- Kalan **3 ms**'nin büyük kısmı işletim sistemi (Windows/Linux) USB sürücü gecikmesi ve Python'un çalışma zamanından kaynaklanmaktadır.
- **Sonuç:** STM32 üzerindeki DEOS yazılımının paketi alması (Interrupt + Semaphore), Router üzerinden yönlendirmesi, PONG cevabını oluşturup kuyruğa (Queue) atması ve TX Thread üzerinden tekrar UART'a basması **1 milisaniyenin (mikrosaniyeler seviyesi) altındadır.**

## 4. LoRa Radyo Performansı (Havadan Test)
Sisteme Ebyte (E22/E32 vb.) LoRa modülleri eklendiğinde elde edilen sonuçlar:

- **UART Baud Rate:** 115200 bps
- **LoRa Air Rate:** 2.4 kbps
- **Ölçülen Ortalama Gidiş-Dönüş Süresi (RTT):** `623.5 ms`

### Analiz ve Kırılım:
- Kablo ile elde ettiğimiz 6 ms'lik süreyi (İşlem + UART aktarımı) çıkardığımızda, **617.5 ms**'lik süre tamamen modüllerin paketi havadan göndermesiyle (Air Time) geçmektedir.
- Bu da tek yönlü bir LoRa aktarımının (Preamble ve Hata Düzeltme - FEC dahil) yaklaşık **308 ms** sürdüğünü göstermektedir.
- Havadan gönderimde sağlanan bu olağanüstü tutarlılık (sapmanın 1 ms'den az olması), donanımın ve antenin tamamen stabil ve parazitsiz (Channel 23) çalıştığını gösterir.

## 5. Yazılımsal Optimizasyonlar
- **Zero-Latency RX:** İlk tasarımdaki UART ring-buffer bekleme döngüsünde yer alan `k_msleep(1)` (Polling) fonksiyonu kaldırılmıştır.
- **Semafor Mimarisi:** Bunun yerine Donanım Kesmesi (Interrupt) tabanlı bir Semaphore (`m_rx_sem`) yapısı kurulmuştur.
- **Kazanç:** UART pinine ilk bayt düştüğü mikrosaniye içerisinde işletim sistemi `rx_thread`'i uyandırmaktadır. Bu sayede işlemci gereksiz yere uyanıp güç tüketmekten kurtulmuş ve işleme süresindeki 1-2 ms'lik rastgele sapmalar tamamen yok edilmiştir.
