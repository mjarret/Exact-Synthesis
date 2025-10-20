def generate_heap_permutations(n, a):
    c = [0] * n
    calls = []  # Collect perform_swap_and_compare calls in a list

    i = 0
    while i < n:
        if c[i] < i:
            if i % 2 == 0:
                swap_indices = (0, i)
            else:
                swap_indices = (c[i], i)

            # Add the lambda call to the list
            calls.append(f"perform_swap_and_compare({swap_indices[0]}, {swap_indices[1]});")

            # Swap elements in array a (not needed for code generation)
            a[swap_indices[0]], a[swap_indices[1]] = a[swap_indices[1]], a[swap_indices[0]]
            c[i] += 1
            i = 0
        else:
            c[i] = 0
            i += 1

    return calls

def main():
    n = 6
    a = list(range(n))
    output = []

    # Begin the function and define the lambda
    output.append("void pattern::canonical_form() {")
    output.append("    uint72_t smallest_pattern = pattern_data;  // Initialize with the current pattern")
    output.append("")
    output.append("    // Define the lambda function")
    output.append("    auto perform_swap_and_compare = [&](int x, int y) {")
    output.append("        swap_rows(x, y);")
    output.append("        sort_columns();")
    output.append("        if (pattern_data < smallest_pattern) {")
    output.append("            smallest_pattern = pattern_data;")
    output.append("        }")
    output.append("    };")
    output.append("")

    # Output the initial processing
    output.append("    // Initial permutation")
    output.append("    sort_columns();")
    output.append("    if (pattern_data < smallest_pattern) {")
    output.append("        smallest_pattern = pattern_data;")
    output.append("    }")

    # Generate the swap calls
    calls = generate_heap_permutations(n, a)

    # Combine all calls into a single line
    calls_line = "    " + " ".join(calls)

    # Append the combined calls line to the output
    output.append(calls_line)

    output.append("")
    output.append("    // Set the pattern to the smallest canonical form")
    output.append("    pattern_data = smallest_pattern;")
    output.append("}")

    # Print the generated code
    print('\n'.join(output))

if __name__ == "__main__":
    main()

