import pandas as pd
import matplotlib.pyplot as plt
import os

def main():
    file_path = "sim_results.csv"
    if not os.path.exists(file_path):
        print(f"Error: {file_path} not found.")
        return

    df = pd.read_csv(file_path)

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 8), sharex=True)

    ax1.plot(df['step'], df['pnl'], color='green', label='PnL (Mark-to-Market)', linewidth=2)
    ax1.set_title('Avellaneda-Stoikov Market Maker Performance', fontsize=14, pad=15)
    ax1.set_ylabel('PnL ($)', fontsize=12)
    ax1.grid(True, alpha=0.3)
    ax1.legend(loc='upper left')

    ax2.plot(df['step'], df['inventory'], color='blue', label='Inventory', linewidth=2)
    ax2.axhline(0, color='black', linewidth=1, linestyle='--')
    ax2.set_xlabel('Simulation Step', fontsize=12)
    ax2.set_ylabel('Inventory (Shares)', fontsize=12)
    ax2.grid(True, alpha=0.3)
    ax2.legend(loc='upper left')

    plt.tight_layout()
    plt.savefig('performance_plot.png', dpi=300)
    print("Plot saved to performance_plot.png")

if __name__ == "__main__":
    main()
