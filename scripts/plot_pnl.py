import pandas as pd
import matplotlib.pyplot as plt
import os
import sys

def main():
    file_path = sys.argv[1] if len(sys.argv) > 1 else "sim_results.csv"
    if not os.path.exists(file_path):
        print(f"Error: {file_path} not found.")
        return

    df = pd.read_csv(file_path, comment='#')
    
    # The summary stats at the bottom of the CSV don't have '#' anymore,
    # so they get parsed as data rows where 'Step' is a string like "PnL: 0".
    # We must force 'Step' to numeric, which turns those string rows into NaNs,
    # and then we drop those NaN rows so the graph plots correctly.
    df['Step'] = pd.to_numeric(df['Step'], errors='coerce')
    df = df.dropna(subset=['Step'])

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 8), sharex=True)

    output_name = os.path.join(os.path.dirname(file_path), "performance_plot.png")
    
    # Text box properties for the stats box at the top right
    props = dict(boxstyle='round', facecolor='black', alpha=0.8, edgecolor='gray')
    text_kwargs = dict(fontsize=10, horizontalalignment='right', verticalalignment='center', multialignment='center', 
                       bbox=props, color='white', fontweight='bold', family='monospace')

    if 'AS PnL' in df.columns and 'SMP PnL' in df.columns:
        output_name = os.path.join(os.path.dirname(file_path), "performance_plot.png")
        ax1.plot(df['Step'], df['AS PnL'], color='red', label='Avellaneda-Stoikov PnL', linewidth=2)
        ax1.plot(df['Step'], df['SMP PnL'], color='green', label='Stoikov Micro-Price PnL', linewidth=2)
        fig.text(0.05, 0.92, 'Trading Bots Strategy Comparison', fontsize=14, fontweight='bold', va='center', ha='left')
        
        final_av = df['AS PnL'].dropna().iloc[-1]
        final_imb = df['SMP PnL'].dropna().iloc[-1]
        winner = "Avellaneda-Stoikov Bot!" if final_av > final_imb else "Stoikov Micro-Price Bot!"
        
        stats_text = f"--- FINAL RESULTS ---\nAvellaneda-Stoikov PnL: ${final_av:,.2f}\nStoikov Micro-Price PnL: ${final_imb:,.2f}\n\nWINNER: {winner}"
        fig.text(0.95, 0.92, stats_text, **text_kwargs)
        
        ax2.plot(df['Step'], df['AS Inventory'], color='red', label='Avellaneda-Stoikov Inventory', linewidth=2)
        ax2.plot(df['Step'], df['SMP Inventory'], color='green', label='Stoikov Micro-Price Inventory', linewidth=2)

    elif 'AS PnL' in df.columns:
        output_name = os.path.join(os.path.dirname(file_path), "performance_plot_AS.png")
        ax1.plot(df['Step'], df['AS PnL'], color='red', label='Avellaneda-Stoikov PnL', linewidth=2)
        fig.text(0.05, 0.92, 'Trading Bot Performance (Avellaneda-Stoikov Bot)', fontsize=14, fontweight='bold', va='center', ha='left')
        
        final_av = df['AS PnL'].dropna().iloc[-1]
        stats_text = f"--- FINAL RESULTS ---\nAvellaneda-Stoikov PnL: ${final_av:,.2f}"
        fig.text(0.95, 0.92, stats_text, **text_kwargs)
        
        ax2.plot(df['Step'], df['AS Inventory'], color='red', label='Avellaneda-Stoikov Inventory', linewidth=2)

    elif 'SMP PnL' in df.columns:
        output_name = os.path.join(os.path.dirname(file_path), "performance_plot_SMP.png")
        ax1.plot(df['Step'], df['SMP PnL'], color='green', label='Stoikov Micro-Price PnL', linewidth=2)
        fig.text(0.05, 0.92, 'Trading Bot Performance (Stoikov Micro-Price Bot)', fontsize=14, fontweight='bold', va='center', ha='left')
        
        final_imb = df['SMP PnL'].dropna().iloc[-1]
        stats_text = f"--- FINAL RESULTS ---\nStoikov Micro-Price PnL: ${final_imb:,.2f}"
        fig.text(0.95, 0.92, stats_text, **text_kwargs)
        
        ax2.plot(df['Step'], df['SMP Inventory'], color='green', label='Stoikov Micro-Price Inventory', linewidth=2)

    ax1.set_ylabel('PnL ($)', fontsize=12)
    ax1.grid(True, alpha=0.3)
    ax1.legend(loc='upper left')

    ax2.axhline(0, color='black', linewidth=1, linestyle='--')
    ax2.set_xlabel('Simulation Step', fontsize=12)
    ax2.set_ylabel('Inventory (Shares)', fontsize=12)
    ax2.grid(True, alpha=0.3)
    ax2.legend(loc='upper left')

    plt.tight_layout(rect=[0, 0, 1, 0.85])
    plt.savefig(output_name, dpi=300)
    print(f"  Plot generated and saved to {output_name}")

if __name__ == "__main__":
    main()
