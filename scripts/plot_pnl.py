import pandas as pd
import matplotlib.pyplot as plt
import os

def main():
    file_path = "sim_results.csv"
    if not os.path.exists(file_path):
        print(f"Error: {file_path} not found.")
        return

    df = pd.read_csv(file_path, comment='#')

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 8), sharex=True)

    output_name = 'performance_plot.png'
    
    # Text box properties to avoid overlap (Bottom Right)
    props = dict(boxstyle='round', facecolor='black', alpha=0.8, edgecolor='gray')
    text_kwargs = dict(transform=ax1.transAxes, fontsize=10,
                       horizontalalignment='right', verticalalignment='bottom', 
                       bbox=props, color='white', fontweight='bold', family='monospace')

    if 'PnL Avellaneda' in df.columns and 'PnL Imbalance' in df.columns:
        output_name = 'performance_plot.png'
        ax1.plot(df['Step'], df['PnL Avellaneda'], color='red', label='Avellaneda-Stoikov PnL', linewidth=2)
        ax1.plot(df['Step'], df['PnL Imbalance'], color='green', label='Imbalance-Aware PnL', linewidth=2)
        ax1.set_title('Market Maker Comparison (PnL)', fontsize=14, pad=15)
        
        final_av = df['PnL Avellaneda'].dropna().iloc[-1]
        final_imb = df['PnL Imbalance'].dropna().iloc[-1]
        winner = "Imbalance-Aware" if final_imb > final_av else "Avellaneda-Stoikov"
        stats_text = f"--- FINAL RESULTS ---\nAvellaneda PnL: ${final_av:,.2f}\nImbalance PnL: ${final_imb:,.2f}\n\nWINNER: {winner}!"
        ax1.text(0.98, 0.05, stats_text, **text_kwargs)
        
        ax2.plot(df['Step'], df['Inventory Avellaneda'], color='red', label='Avellaneda-Stoikov Inventory', linewidth=2, alpha=0.7)
        ax2.plot(df['Step'], df['Inventory Imbalance'], color='green', label='Imbalance-Aware Inventory', linewidth=2, alpha=0.7)

    elif 'PnL Avellaneda' in df.columns:
        output_name = 'performance_plot_avellaneda.png'
        ax1.plot(df['Step'], df['PnL Avellaneda'], color='red', label='Avellaneda-Stoikov PnL', linewidth=2)
        ax1.set_title('Market Maker Performance (Avellaneda-Stoikov 2008)', fontsize=14, pad=15)
        
        final_av = df['PnL Avellaneda'].dropna().iloc[-1]
        stats_text = f"--- FINAL RESULTS ---\nAvellaneda PnL: ${final_av:,.2f}"
        ax1.text(0.98, 0.05, stats_text, **text_kwargs)
        
        ax2.plot(df['Step'], df['Inventory Avellaneda'], color='red', label='Avellaneda-Stoikov Inventory', linewidth=2)

    elif 'PnL Imbalance' in df.columns:
        output_name = 'performance_plot_imbalance.png'
        ax1.plot(df['Step'], df['PnL Imbalance'], color='green', label='Imbalance-Aware PnL', linewidth=2)
        ax1.set_title('Market Maker Performance (Imbalance-Aware 2018)', fontsize=14, pad=15)
        
        final_imb = df['PnL Imbalance'].dropna().iloc[-1]
        stats_text = f"--- FINAL RESULTS ---\nImbalance PnL: ${final_imb:,.2f}"
        ax1.text(0.98, 0.05, stats_text, **text_kwargs)
        
        ax2.plot(df['Step'], df['Inventory Imbalance'], color='green', label='Imbalance-Aware Inventory', linewidth=2)

    ax1.set_ylabel('PnL ($)', fontsize=12)
    ax1.grid(True, alpha=0.3)
    ax1.legend(loc='upper left')

    ax2.axhline(0, color='black', linewidth=1, linestyle='--')
    ax2.set_xlabel('Simulation Step', fontsize=12)
    ax2.set_ylabel('Inventory (Shares)', fontsize=12)
    ax2.grid(True, alpha=0.3)
    ax2.legend(loc='upper left')

    plt.tight_layout()
    plt.savefig(output_name, dpi=300)
    print(f"Plot saved to {output_name}")

if __name__ == "__main__":
    main()
