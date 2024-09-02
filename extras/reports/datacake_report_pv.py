###################################################################################################
# datacake_report_pv.py
#
# This script generates a PDF report from CSV files containing PV inverter data
# which are provided by Datacake.
#
# created: 09/2024
#
#
# MIT License
#
# Copyright (c) 2024 Matthias Prinke
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#
#
# History:
#
# 20240902 Created
#
# ToDo:
# -
###################################################################################################

import os
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from matplotlib.backends.backend_pdf import PdfPages

COLUMNS = {
    'power': 0,
    'energytoday': 2,
    'energytotal': 3,

}

COLORS = {
    'power': 'r',
    'energytoday': 'lime',
    'energytotal': 'limegreen',
}

WIDTH = 18  / 2.54
HEIGHT = 27 / 2.54

# Directory containing the CSV files
src_dir = "/home/mp/datacake/PV-Inverter/2024"
pdf_file = "/home/mp/datacake/PV-Inverter/pv_inverter_2024.pdf"
last_column = 4

# List all CSV files in the directory
csv_files = [f for f in os.listdir(src_dir) if f.endswith('.csv')]

# Initialize an empty list to store DataFrames
dataframes = []

# Read each CSV file
for file in csv_files:
    file_path = os.path.join(src_dir, file)
    df = pd.read_csv(file_path, skiprows=0, parse_dates=[0], date_parser=lambda x: pd.to_datetime(x, format='%a, %d %b %Y %H:%M:%S'))
    
    for col in range(1, last_column):
        # Replace comma with dot and convert to float
        df.iloc[:, col] = df.iloc[:, col].astype(float)

    dataframes.append(df)

# Combine all DataFrames into a single DataFrame
combined_df = pd.concat(dataframes, ignore_index=True)

# Set the first column as the index (time)
combined_df.set_index(combined_df.columns[0], inplace=True)

# Sort the DataFrame by the index (time)
combined_df.sort_index(inplace=True)

# Print the combined DataFrame
print(combined_df)

# Remove invalid rows from the combined DataFrame
combined_df.dropna(inplace=True)

def title_page(plt, pdf, title, font_size=24):
    """Create a title page with the given title."""
    fig, ax = plt.subplots(figsize=(WIDTH, HEIGHT))
    ax.text(0.5, 0.5, title, transform=ax.transAxes, fontsize=font_size, ha='center', va='center')
    ax.axis('off')
    pdf.savefig(fig)
    plt.close(fig)

def plot_data(plt, pdf, df, columns, title, xlabel, ylabels, colors, avg_label, avg_colors):
    """Plot the data from the DataFrame."""
    fig, axes = plt.subplots(len(columns), 1, figsize=(WIDTH, HEIGHT), layout="tight")
    for i in range(len(columns)):
        if len(columns) == 1:
            ax = axes
        else:
            ax = axes[i]
        ax.plot(df.index, df.iloc[:, columns[i]], label=ylabels[i], color=colors[i])
        if xlabel:
            ax.set_xlabel(xlabel)
        ax.set_ylabel(ylabels[i])
        ax.xaxis.set_major_formatter(mdates.DateFormatter('%d-%m-%y %H:%M'))
        ax.xaxis.set_major_locator(mdates.AutoDateLocator())
        if i < len(columns) - 1:
            # Remove x-axis labels for all but the last subplot
            ax.set_xticklabels([])
        if avg_colors[i]:
            # Calculate daily average and plot as dashed line
            daily_avg = df.iloc[:, columns[i]].resample('D').mean()
            ax.plot(daily_avg.index, daily_avg, label=f'{avg_label} {ylabels[i]}', color=avg_colors[i], linestyle='--', linewidth=2)

        ax.legend()
        ax.grid(True)
        
        # Rotate x-axis labels for better readability
        plt.setp(ax.xaxis.get_majorticklabels(), rotation=45)

    fig.suptitle(title, fontsize=16)

    pdf.savefig(fig)
    plt.close(fig)


# Create a PdfPages object to save the figures
with PdfPages(pdf_file) as pdf:
    title_page(plt, pdf, 'PV-Inverter 2024')

    title_page(plt, pdf, 'Yearly Overview', 18)

    plot_data(plt, pdf, combined_df, 
              [COLUMNS['power'], COLUMNS['energytoday'], COLUMNS['energytotal']], 
              'PV-Inverter', None, ['Power [W]', 'Energy today [Wh]', 'Energy total [Wh]'],
              [COLORS['power'], COLORS['energytoday'], COLORS['energytotal']], 
              'Daily average', [None, None, None])

    title_page(plt, pdf, 'Monthly Reports', 18)

    # Group the data by month
    combined_df['Month'] = combined_df.index.to_period('M')

    # Create a separate figure for each month
    for month, month_df in combined_df.groupby('Month'):
        title_page(plt, pdf, month, 16)

        plot_data(plt, pdf, month_df,
                  [COLUMNS['power'], COLUMNS['energytoday'], COLUMNS['energytotal']],
                  f'PV-Inverter {month}', None, ['Power [W]', 'Energy today [Wh]', 'Energy total [Wh]'],
                  [COLORS['power'], COLORS['energytoday'], COLORS['energytotal']],
                  'Daily average', [None, None, None])
