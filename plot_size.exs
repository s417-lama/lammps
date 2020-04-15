linewidth = 2

# intvl = 1
intvl = 2
# intvl = 3
thread = 55
sizes = [2, 4, 6, 8, 10, 12]
natoms = [256000, 2048000, 6912000, 16384000, 32000000, 55296000]

result_dir = "~/lammps/results/latest"

data = [
  {"omp_async"     , "Pthread"           , "#d62728", "dash"   , "x-thin"     },
  {"abt_sync"      , "Non-preemptive ULT", "#1f77b4", "dot"    , "square-open"},
  {"abt_preemption", "Preemptive ULT"    , "#2ca02c", "solid"  , "circle"     },
]

get_median_of_files = fn path ->
  path
  |> Path.expand()
  |> Path.wildcard()
  |> Enum.map(fn path ->
    File.stream!(path)
    |> Stream.map(&String.split/1)
    |> Stream.map(fn
      ["Loop", "time", "of", time | _] -> Float.parse(time)
      _                                -> nil
    end)
    |> Stream.filter(&!is_nil(&1))
    |> Enum.to_list()
    |> case do
      [{time, ""}] -> time
      _            -> nil
    end
  end)
  |> Statistics.median()
end

baselines =
  Enum.map(sizes, fn size ->
    get_median_of_files.("#{result_dir}/abt_no_analysis_*_size_#{size}_*.out")
  end)

data
|> Enum.map(fn {prefix, title, color, dash, symbol} ->
  ys =
    Enum.zip(sizes, baselines)
    |> Enum.map(fn {size, baseline} ->
      m = get_median_of_files.("#{result_dir}/#{prefix}_intvl_#{intvl}_thread_#{thread}_size_#{size}_*.out")
      m / baseline - 1
    end)
  %{
    x: natoms,
    y: ys,
    type: "scatter",
    name: title,
    line: %{
      color: color,
      width: linewidth,
      dash: dash,
    },
    marker: %{
      symbol: symbol,
      size:   12,
      line:   %{width: linewidth, color: color},
    },
  }
end)
|> PlotlyEx.plot(%{
  width:  400,
  height: 350,
  xaxis: %{
    title: %{text: "# of atoms"},
    showline: true,
    exponentformat: "power",
    range: [0, Enum.max(natoms) * 1.05],
  },
  yaxis: %{
    title: %{text: "Overhead"},
    showline: true,
    # range: [0, 2],
    range: [0, 1],
  },
  font: %{
    size: 22,
  },
  legend: %{
    x: 0.4,
    y: 1.0,
  },
  margin: %{
    l: 70,
    r: 0,
    b: 70,
    t: 10,
    pad: 0,
  },
})
|> PlotlyEx.show()
