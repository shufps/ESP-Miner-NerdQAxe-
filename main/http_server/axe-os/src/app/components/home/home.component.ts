import { Component, OnInit, OnDestroy } from '@angular/core';
import { interval, map, Observable, shareReplay, startWith, switchMap, tap, from } from 'rxjs';
import { concatMap } from 'rxjs/operators';
import { HashSuffixPipe } from 'src/app/pipes/hash-suffix.pipe';
import { SystemService } from 'src/app/services/system.service';
import { ISystemInfo } from 'src/models/ISystemInfo';
import { Chart } from 'chart.js';  // Import Chart.js

@Component({
  selector: 'app-home',
  templateUrl: './home.component.html',
  styleUrls: ['./home.component.scss']
})
export class HomeComponent implements OnInit, OnDestroy {

  public info$: Observable<ISystemInfo>;
  public quickLink$: Observable<string | undefined>;
  public expectedHashRate$: Observable<number | undefined>;

  public chartOptions: any;
  public dataLabel: number[] = [];
  public dataData: number[] = [];
  public dataData10m: number[] = [];
  public dataData1h: number[] = [];
  public dataData1d: number[] = [];
  public chartData?: any;

  private localStorageKey = 'chartData';
  private timestampKey = 'lastTimestamp'; // Key to store lastTimestamp

  constructor(
    private systemService: SystemService
  ) {
    const documentStyle = getComputedStyle(document.documentElement);
    const textColor = documentStyle.getPropertyValue('--text-color');
    const textColorSecondary = documentStyle.getPropertyValue('--text-color-secondary');
    const surfaceBorder = documentStyle.getPropertyValue('--surface-border');
    const primaryColor = documentStyle.getPropertyValue('--primary-color');

    this.chartData = {
      labels: [],
      datasets: [
        {
          type: 'line',
          label: 'Hashrate 10m',
          data: this.dataData10m,
          fill: false,
          backgroundColor: '#6484f6',
          borderColor: '#6484f6',
          tension: .4,
          pointRadius: 1,
          borderWidth: 1
        },
        {
          type: 'line',
          label: 'Hashrate 1h',
          data: this.dataData1h,
          fill: false,
          backgroundColor: '#7464f6',
          borderColor: '#7464f6',
          tension: .4,
          pointRadius: 1,
          borderWidth: 1
        },
        {
          type: 'line',
          label: 'Hashrate 1d',
          data: this.dataData1d,
          fill: false,
          backgroundColor: '#a564f6',
          borderColor: '#a564f6',
          tension: .4,
          pointRadius: 1,
          borderWidth: 1
        },
      ]
    };

    this.chartOptions = {
      animation: false,
      maintainAspectRatio: false,
      plugins: {
        legend: {
          labels: {
            color: textColor
          }
        }
      },
      scales: {
        x: {
          type: 'time',
          time: {
            unit: 'hour',
          },
          ticks: {
            color: textColorSecondary
          },
          grid: {
            color: surfaceBorder,
            drawBorder: false,
            display: true
          }
        },
        y: {
          ticks: {
            color: textColorSecondary,
            callback: (value: number) => HashSuffixPipe.transform(value)
          },
          grid: {
            color: surfaceBorder,
            drawBorder: false
          }
        }
      }
    };

    this.loadChartData(); // Load chart data and lastTimestamp from local storage

    this.info$ = interval(5000).pipe(
      startWith(() => this.systemService.getInfo()),
      switchMap(() => this.systemService.getInfo()),
      tap(info => {
        const now = new Date().getTime();

        // Update chartData here...

        // Trigger chart update by assigning the chartData to itself
        this.chartData = {
          ...this.chartData
        };

      }),
      map(info => {
        info.power = parseFloat(info.power.toFixed(1));
        info.voltage = parseFloat((info.voltage / 1000).toFixed(1));
        info.current = parseFloat((info.current / 1000).toFixed(1));
        info.coreVoltageActual = parseFloat((info.coreVoltageActual / 1000).toFixed(2));
        info.coreVoltage = parseFloat((info.coreVoltage / 1000).toFixed(2));
        info.temp = parseFloat(info.temp.toFixed(1));
        info.vrTemp = parseFloat(info.vrTemp.toFixed(1));

        return info;
      }),
      shareReplay({ refCount: true, bufferSize: 1 })
    );

    this.expectedHashRate$ = this.info$.pipe(map(info => {
      return Math.floor(info.frequency * ((info.smallCoreCount * info.asicCount) / 1000));
    }));

    this.quickLink$ = this.info$.pipe(
      map(info => {
        if (info.stratumURL.includes('public-pool.io')) {
          const address = info.stratumUser.split('.')[0];
          return `https://web.public-pool.io/#/app/${address}`;
        } else if (info.stratumURL.includes('ocean.xyz')) {
          const address = info.stratumUser.split('.')[0];
          return `https://ocean.xyz/stats/${address}`;
        } else if (info.stratumURL.includes('solo.d-central.tech')) {
          const address = info.stratumUser.split('.')[0];
          return `https://solo.d-central.tech/#/app/${address}`;
        } else if (info.stratumURL.includes('solo.ckpool.org')) {
          const address = info.stratumUser.split('.')[0];
          return `https://solostats.ckpool.org/stats/${address}`;
        } else {
          return undefined;
        }
      })
    );
  }

  ngOnInit(): void {
    this.systemService.getHistoryLen().subscribe({
        next: (data) => {
            console.log('History Length Data:', data);
            // Don't clear chart data immediately, preserve existing data
            // this.clearChartData();

            const storedLastTimestamp = this.getStoredTimestamp(); // Returns microseconds if stored
            const currentTimestamp = new Date().getTime() * 1000; // Current time in microseconds
            const oneHourAgo = currentTimestamp - 3600 * 1000 * 1000; // 1 hour in microseconds

            console.log("stored timestamp: " + storedLastTimestamp);

            // Calculate the start timestamp based on stored data, ensuring not to fetch more than one hour
            let startTimestamp = storedLastTimestamp ? storedLastTimestamp + 1 : oneHourAgo;
            console.log("start fetching timestamp: " + startTimestamp);

            // Determine the end timestamp to ensure data is not fetched beyond current time
            const endTimestamp = Math.min(data.lastTimestamp, currentTimestamp);
            console.log("end fetching timestamp: " + endTimestamp);

            // Fetch data only if there is a valid range
            if (startTimestamp < endTimestamp) {
                this.fetchHistoricalData(startTimestamp, endTimestamp);
            } else {
                console.log('No new data to fetch');
            }
        },
        error: (err) => {
            console.error('Failed to fetch history length:', err);
        }
    });
  }

  ngOnDestroy(): void {
    // Optionally clear any intervals or subscriptions if necessary
  }

  private fetchHistoricalData(startTimestamp: number, lastTimestamp: number): void {
    const requests: Observable<any>[] = [];
    requests.push(this.systemService.getHistoryData(startTimestamp));
    console.log("Fetching historical data from " + startTimestamp);

    from(requests).pipe(
      concatMap((request: Observable<any>) => request),
      tap(data => {
        console.log('Received Historical Data:', data);
        this.updateChartData(data); // Append new data to existing data

        // Store lastTimestamp after fetching data
        if (data.timestamps && data.timestamps.length) {
          const lastDataTimestamp = Math.max(...data.timestamps);
          this.storeTimestamp(lastDataTimestamp);
          console.log("store new timestamp: "+lastDataTimestamp);
          this.saveChartData(); // Save updated chart data to local storage
        }
      })
    ).subscribe({
      complete: () => {
        console.log('All historical data has been retrieved.');
      }
    });
  }

  private clearChartData(): void {
    this.dataLabel = [];
    this.dataData10m = [];
    this.dataData1h = [];
    this.dataData1d = [];
  }

  private updateChartData(data: any): void {
    // Convert timestamps from microseconds to milliseconds for chart.js
    const convertedTimestamps = data.timestamps.map((ts: number) => ts / 1000);
    const convertedhashrate_10m = data.hashrate_10m.map((hr: number) => hr * 1000000000);
    const convertedhashrate_1h = data.hashrate_1h.map((hr: number) => hr * 1000000000);
    const convertedhashrate_1d = data.hashrate_1d.map((hr: number) => hr * 1000000000);

    // Append the new data to the existing data arrays
    this.dataLabel = [...this.dataLabel, ...convertedTimestamps];
    this.dataData10m = [...this.dataData10m, ...convertedhashrate_10m];
    this.dataData1h = [...this.dataData1h, ...convertedhashrate_1h];
    this.dataData1d = [...this.dataData1d, ...convertedhashrate_1d];

    this.chartData.labels = this.dataLabel;
    this.chartData.datasets[0].data = this.dataData10m;
    this.chartData.datasets[1].data = this.dataData1h;
    this.chartData.datasets[2].data = this.dataData1d;

    this.filterOldData(); // Ensure old data is removed
  }

  private loadChartData(): void {
    const storedData = localStorage.getItem(this.localStorageKey);
    if (storedData) {
      const parsedData = JSON.parse(storedData);
      this.dataLabel = parsedData.labels || [];
      this.dataData = parsedData.dataData || [];
      this.dataData10m = parsedData.dataData10m || [];
      this.dataData1h = parsedData.dataData1h || [];
      this.dataData1d = parsedData.dataData1d || [];

      this.filterOldData(); // Remove old data after loading
    }
  }

  private saveChartData(): void {
    const dataToSave = {
      labels: this.dataLabel,
      dataData: this.dataData,
      dataData10m: this.dataData10m,
      dataData1h: this.dataData1h,
      dataData1d: this.dataData1d
    };
    localStorage.setItem(this.localStorageKey, JSON.stringify(dataToSave));
  }

  private filterOldData(): void {
/*
    const now = new Date().getTime() * 1000; // Current time in microseconds
    const cutoff = now - 3600 * 1000000; // 1 hour in microseconds

    while (this.dataLabel.length && this.dataLabel[0] < cutoff / 1000) { // Convert cutoff to milliseconds for comparison
      this.dataLabel.shift();
      this.dataData.shift();
      this.dataData10m.shift();
      this.dataData1h.shift();
      this.dataData1d.shift();
    }

    // Update the stored timestamp based on the oldest remaining data point
    if (this.dataLabel.length) {
      this.storeTimestamp(this.dataLabel[0] * 1000); // Convert back to µs for storage
    }
*/
  }

  private storeTimestamp(timestamp: number): void {
    console.log("store new timestamp: " + timestamp);
    localStorage.setItem(this.timestampKey, timestamp.toString());
  }

  private getStoredTimestamp(): number | null {
    const storedTimestamp = localStorage.getItem(this.timestampKey);
    if (storedTimestamp) {
      const timestamp = parseInt(storedTimestamp, 10);
      console.log("loaded stored timestamp: " + timestamp);
      return timestamp;
    }
    return null;
  }
}
